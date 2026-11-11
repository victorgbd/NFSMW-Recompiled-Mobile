// nfsmw - Lossless Scaling frame generation in the native renderer (cvar nfsmw_lsfg)
//
// ===========================================================================
//  WHAT IT IS
//
//  Lossless Scaling's frame generation (LSFG 3.1) makes up the frames between two real ones from the images alone
//  (optical flow): it needs no depth or motion vectors from the game. Its shaders belong to Lossless Scaling, a paid
//  program: they are not included. Each user copies their own Lossless.dll to the phone (the launcher puts it in
//  the app's files and passes nfsmw_lsfg_dll). The code that runs them is lsfg-vk v1.0.0's framegen (MIT,
//  thirdparty/lsfg-vk), with its shaders read from the DLL by pe-parse (MIT) and translated from DXBC to SPIR-V by
//  DXVK's compiler (zlib, thirdparty/dxbc). Changes to framegen: it runs on this renderer's Vulkan device instead
//  of creating its own (on Android it would end up on another driver than Turnip), its submissions take the
//  queue's lock and the renderer draws straight into its inputs (marked NFSMW there).
//
//  HOW A FRAME GOES
//  On the ring thread, at each Swap:
//    1. The usual output pass (gamma ramp, post-processing, GSR 2 if it is on) draws the frame into an image of
//       ours (cuadro_) instead of the presenter's.
//    2. A copy pass turns it into framegen's next input (R8G8B8A8, its storage format).
//    3. framegen generates (multiplier - 1) frames between the previous input and this one, on the GPU.
//    4. The generated frames and the real one go to the pacing thread.
//  The pacing thread presents them spread over the measured interval between Swaps: the first generated one at
//  once, the real one (multiplier - 1) / multiplier of an interval later. So everything shown is one real frame
//  late (that is the latency frame generation adds). If a new Swap arrives before it has finished, it presents the
//  real frame at once and drops the rest of the generated ones.
//
//  WHEN IT MAKES SENSE
//  With the game below the screen's refresh rate (the 30 fps limit on a 60 Hz screen, or 60 on 120 Hz). The
//  presenter shows the latest image at each refresh: frames closer together than the refresh are not seen.
//
//  WITHOUT TESTING ON A PHONE (written 2026-11-11). Needs Vulkan 1.3 with synchronization2, vulkanMemoryModel and
//  nullDescriptor (the log says what is missing). What to look at first: the "[lsfg]" lines.
// ===========================================================================

#include "nfsmw_lsfg.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/presenter.h>
#include <rex/ui/vulkan/device.h>
#include <rex/ui/vulkan/presenter.h>
#include <rex/ui/vulkan/util.h>

#include "extract/extract.hpp"
#include "extract/trans.hpp"
#include "lsfg_nfsmw.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

REXCVAR_DEFINE_BOOL(nfsmw_lsfg, false, "NFSMW",
                    "Generacion de fotogramas de Lossless Scaling (LSFG 3.1) en el renderizador nativo (Vulkan). "
                    "Necesita el Lossless.dll del usuario (nfsmw_lsfg_dll)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_STRING(nfsmw_lsfg_dll, "", "NFSMW", "Ruta del Lossless.dll del usuario (Lossless Scaling)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(nfsmw_lsfg_multiplicador, 2, "NFSMW",
                     "Generacion de fotogramas: fotogramas mostrados por cada uno del juego (2 = x2: uno inventado "
                     "entre cada dos reales)")
    .range(2, 4)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_DOUBLE(nfsmw_lsfg_flujo, 0.5, "NFSMW",
                      "Generacion de fotogramas: resolucion del calculo de movimiento (0.25-1; menos es mas rapido)")
    .range(0.25, 1.0)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(nfsmw_lsfg_rendimiento, true, "NFSMW",
                    "Generacion de fotogramas: el modo de rendimiento de LSFG 3.1 (mas ligero, algo peor)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace nfsmw::lsfg {
namespace {

namespace shaders {
#include "vulkan_spirv/guest_output_bilinear_ps.h"
}  // namespace shaders

using rex::ui::vulkan::VulkanDevice;
using rex::ui::vulkan::VulkanPresenter;
using Reloj = std::chrono::steady_clock;

constexpr VkFormat kFormatoLsfg = VK_FORMAT_R8G8B8A8_UNORM;
constexpr uint32_t kComandosConversion = 3;
constexpr uint32_t kComandosMostrar = 4;
constexpr size_t kFramebuffersMostrar = 6;

// framegen's submissions: the queue is shared with the renderer and the presenter.
const VulkanDevice* g_dispositivo = nullptr;
uint32_t g_familia = 0;
VkResult EnviarACola(VkQueue, uint32_t cuantos, const VkSubmitInfo* envios, VkFence valla) {
  const auto cola = g_dispositivo->AcquireQueue(g_familia, 0);
  return g_dispositivo->functions().vkQueueSubmit(cola.queue(), cuantos, envios, valla);
}

struct Comandos {
  VkCommandPool pool = VK_NULL_HANDLE;  // one per buffer: the SDK does not load vkResetCommandBuffer
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  VkFence valla = VK_NULL_HANDLE;
  bool pendiente = false;
  VkDescriptorSet conjunto = VK_NULL_HANDLE;
};

struct Entrega {
  rex::ui::Presenter* presentador = nullptr;
  std::vector<VkImageView> generadas;
  VkImageView real = VK_NULL_HANDLE;
  uint32_t ancho = 0, alto = 0, aspecto_x = 0, aspecto_y = 0;
  Reloj::time_point llegada;
  double intervalo_ms = 33.3;
};

class LsfgVulkanImpl final : public LsfgVulkan {
 public:
  LsfgVulkanImpl(const VulkanDevice* dispositivo, const RecursosSalida& recursos)
      : dispositivo_(dispositivo),
        dfn_(dispositivo->functions()),
        device_(dispositivo->device()),
        familia_(dispositivo->queue_family_graphics_compute()),
        rec_(recursos),
        generadas_(uint32_t(REXCVAR_GET(nfsmw_lsfg_multiplicador)) - 1) {}

  ~LsfgVulkanImpl() override {
    {
      std::lock_guard<std::mutex> candado(m_);
      salir_ = true;
    }
    cv_.notify_all();
    if (hilo_.joinable()) {
      hilo_.join();
    }
    EsperarComandos(conversion_);
    EsperarComandos(mostrar_);
    try {
      LSFG_NFSMW::finalize();
    } catch (const std::exception& e) {
      REXLOG_WARN("[lsfg] al cerrar: {}", e.what());
    }
    DestruirTamano();
    for (auto& f : framebuffers_mostrar_) {
      dfn_.vkDestroyFramebuffer(device_, f.second, nullptr);
    }
    for (Comandos& c : conversion_) {
      if (c.valla != VK_NULL_HANDLE) dfn_.vkDestroyFence(device_, c.valla, nullptr);
      if (c.pool != VK_NULL_HANDLE) dfn_.vkDestroyCommandPool(device_, c.pool, nullptr);
    }
    for (Comandos& c : mostrar_) {
      if (c.valla != VK_NULL_HANDLE) dfn_.vkDestroyFence(device_, c.valla, nullptr);
      if (c.pool != VK_NULL_HANDLE) dfn_.vkDestroyCommandPool(device_, c.pool, nullptr);
    }
    if (pool_descriptores_ != VK_NULL_HANDLE) dfn_.vkDestroyDescriptorPool(device_, pool_descriptores_, nullptr);
    if (pipeline_conversion_ != VK_NULL_HANDLE) dfn_.vkDestroyPipeline(device_, pipeline_conversion_, nullptr);
    if (fs_bilineal_ != VK_NULL_HANDLE) dfn_.vkDestroyShaderModule(device_, fs_bilineal_, nullptr);
    if (render_pass_lsfg_ != VK_NULL_HANDLE) dfn_.vkDestroyRenderPass(device_, render_pass_lsfg_, nullptr);
  }

  bool Iniciar() {
    const auto& p = dispositivo_->properties();
    std::string falta;
    if (p.apiVersion < VK_MAKE_API_VERSION(0, 1, 3, 0)) falta += " Vulkan 1.3";
    if (!p.synchronization2) falta += " synchronization2";
    if (!p.vulkanMemoryModel) falta += " vulkanMemoryModel";
    if (!p.nullDescriptor) falta += " nullDescriptor (VK_EXT_robustness2)";
    if (!falta.empty()) {
      REXLOG_WARN("[lsfg] este dispositivo no tiene lo que necesita la generacion de fotogramas:{}", falta);
      return false;
    }
    if (!p.shaderStorageImageWriteWithoutFormat) {
      REXLOG_WARN("[lsfg] sin shaderStorageImageWriteWithoutFormat: se intenta igual (Vulkan 1.3 lo permite por "
                  "formato)");
    }
    const std::string dll = REXCVAR_GET(nfsmw_lsfg_dll);
    if (dll.empty()) {
      REXLOG_WARN("[lsfg] falta el Lossless.dll (nfsmw_lsfg_dll): sin generacion de fotogramas");
      return false;
    }
    try {
      Extract::extractShaders(dll);
    } catch (const std::exception& e) {
      REXLOG_WARN("[lsfg] no se pudieron leer los shaders de {}: {}", dll, e.what());
      return false;
    }

    // framegen on this device.
    g_dispositivo = dispositivo_;
    g_familia = familia_;
    LSFG_NFSMW::Device d;
    d.instance = dispositivo_->vulkan_instance()->instance();
    d.getInstanceProcAddr = dispositivo_->vulkan_instance()->functions().vkGetInstanceProcAddr;
    d.physicalDevice = dispositivo_->physical_device();
    d.device = device_;
    d.queueFamily = familia_;
    d.queue = dispositivo_->AcquireQueue(familia_, 0).queue();
    d.submit = &EnviarACola;
    const bool rendimiento = REXCVAR_GET(nfsmw_lsfg_rendimiento);
    const float flujo = float(REXCVAR_GET(nfsmw_lsfg_flujo));
    try {
      LSFG_NFSMW::initialize(d, rendimiento, flujo, generadas_, [](const std::string& nombre) {
        return Extract::translateShader(Extract::getShader(nombre));
      });
    } catch (const std::exception& e) {
      REXLOG_WARN("[lsfg] no se pudo iniciar framegen: {}", e.what());
      return false;
    }

    // Our objects: the copy pass into framegen's input and the command buffers of both threads.
    VkAttachmentDescription adjunto{};
    adjunto.format = kFormatoLsfg;
    adjunto.samples = VK_SAMPLE_COUNT_1_BIT;
    adjunto.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adjunto.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    adjunto.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adjunto.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adjunto.initialLayout = VK_IMAGE_LAYOUT_GENERAL;  // framegen keeps its images in GENERAL
    adjunto.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentReference referencia{0, VK_IMAGE_LAYOUT_GENERAL};
    VkSubpassDescription subpase{};
    subpase.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpase.colorAttachmentCount = 1;
    subpase.pColorAttachments = &referencia;
    VkRenderPassCreateInfo info_rp{};
    info_rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info_rp.attachmentCount = 1;
    info_rp.pAttachments = &adjunto;
    info_rp.subpassCount = 1;
    info_rp.pSubpasses = &subpase;
    if (dfn_.vkCreateRenderPass(device_, &info_rp, nullptr, &render_pass_lsfg_) != VK_SUCCESS) {
      return false;
    }
    fs_bilineal_ = rex::ui::vulkan::util::CreateShaderModule(dispositivo_, shaders::guest_output_bilinear_ps,
                                                             sizeof(shaders::guest_output_bilinear_ps));
    if (fs_bilineal_ == VK_NULL_HANDLE) {
      return false;
    }
    pipeline_conversion_ = CrearPipeline(render_pass_lsfg_, fs_bilineal_);
    if (pipeline_conversion_ == VK_NULL_HANDLE) {
      return false;
    }
    const uint32_t conjuntos = kComandosConversion + kComandosMostrar;
    VkDescriptorPoolSize tamanos[3] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, conjuntos},
                                       {VK_DESCRIPTOR_TYPE_SAMPLER, conjuntos},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, conjuntos}};
    VkDescriptorPoolCreateInfo info_pool{};
    info_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info_pool.maxSets = conjuntos;
    info_pool.poolSizeCount = 3;
    info_pool.pPoolSizes = tamanos;
    if (dfn_.vkCreateDescriptorPool(device_, &info_pool, nullptr, &pool_descriptores_) != VK_SUCCESS) {
      return false;
    }
    if (!CrearComandos(conversion_.data(), kComandosConversion) || !CrearComandos(mostrar_.data(), kComandosMostrar)) {
      return false;
    }
    hilo_ = std::thread([this] { Mostrador(); });
    REXLOG_INFO("[lsfg] listo: x{} ({} inventado{} por fotograma), LSFG 3.1{}, flujo {:.2f}, shaders de {}",
                generadas_ + 1, generadas_, generadas_ == 1 ? "" : "s", rendimiento ? " rendimiento" : "", flujo,
                dll);
    return true;
  }

  bool PrepararCuadro(uint32_t ancho, uint32_t alto, VkImageView& vista, uint64_t& version) override {
    if (fallido_ || !ancho || !alto) {
      return false;
    }
    if (ancho != ancho_ || alto != alto_) {
      if (!CrearTamano(ancho, alto)) {
        fallido_ = true;
        DestruirTamano();
        REXLOG_WARN("[lsfg] apagado para el resto de la sesion: se presenta como siempre");
        return false;
      }
    }
    vista = cuadro_.vista;
    version = version_cuadro_;
    return true;
  }

  void Generar(rex::ui::Presenter* presentador, uint32_t aspecto_x, uint32_t aspecto_y) override {
    const auto ahora = Reloj::now();
    if (ultima_ != Reloj::time_point{}) {
      const double ms = std::chrono::duration<double, std::milli>(ahora - ultima_).count();
      // The interval between Swaps, smoothed. A pause or a load resets it.
      intervalo_ms_ = ms > 150.0 ? 33.3 : std::clamp(intervalo_ms_ * 0.8 + ms * 0.2, 4.0, 100.0);
    }
    ultima_ = ahora;

    // 1. Copy the frame into framegen's next input.
    VkImageView vista_real = VK_NULL_HANDLE;
    if (!fallido_) {
      try {
        const LSFG_NFSMW::Image entrada = LSFG_NFSMW::nextInput();
        const uint32_t i = entrada.image == entradas_[0].image ? 0 : 1;
        vista_real = entradas_[i].view;
        Convertir(i);
      } catch (const std::exception& e) {
        REXLOG_WARN("[lsfg] {}: apagado", e.what());
        fallido_ = true;
      }
    }
    // 2. The previous frame's presentation is finished (or cut short) before framegen overwrites its outputs.
    EsperarMostrador();
    // 3. Generate.
    std::vector<VkImageView> generadas;
    if (!fallido_) {
      try {
        LSFG_NFSMW::present();
        if (!primero_) {  // the first one has no previous frame to start from
          for (const auto& salida : LSFG_NFSMW::outputs()) {
            generadas.push_back(salida.view);
          }
        }
      } catch (const std::exception& e) {
        REXLOG_WARN("[lsfg] al generar: {}: apagado", e.what());
        fallido_ = true;
      }
      primero_ = false;
    }
    if (vista_real == VK_NULL_HANDLE) {
      return;  // nothing to show; the renderer presents the next frame as always (PrepararCuadro says false)
    }
    // 4. To the pacing thread.
    Entrega e;
    e.presentador = presentador;
    e.generadas = std::move(generadas);
    e.real = vista_real;
    e.ancho = ancho_;
    e.alto = alto_;
    e.aspecto_x = aspecto_x;
    e.aspecto_y = aspecto_y;
    e.llegada = ahora;
    e.intervalo_ms = intervalo_ms_;
    {
      std::lock_guard<std::mutex> candado(m_);
      pendiente_ = std::move(e);
      hay_pendiente_ = true;
    }
    cv_.notify_all();
    if (++fotogramas_ == 1 || fotogramas_ % 1800 == 0) {
      REXLOG_INFO("[lsfg] {} fotogramas del juego a {}x{}, intervalo {:.1f} ms; {} recortados por llegar el "
                  "siguiente antes de tiempo",
                  fotogramas_, ancho_, alto_, intervalo_ms_, recortados_.load(std::memory_order_relaxed));
    }
  }

 private:
  struct ImagenPropia {
    VkImage imagen = VK_NULL_HANDLE;
    VkDeviceMemory memoria = VK_NULL_HANDLE;
    VkImageView vista = VK_NULL_HANDLE;
  };

  bool CrearComandos(Comandos* comandos, uint32_t cuantos) {
    for (uint32_t i = 0; i < cuantos; ++i) {
      VkCommandPoolCreateInfo info_pool{};
      info_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      info_pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
      info_pool.queueFamilyIndex = familia_;
      if (dfn_.vkCreateCommandPool(device_, &info_pool, nullptr, &comandos[i].pool) != VK_SUCCESS) {
        return false;
      }
      VkCommandBufferAllocateInfo reserva{};
      reserva.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      reserva.commandPool = comandos[i].pool;
      reserva.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      reserva.commandBufferCount = 1;
      VkFenceCreateInfo info_valla{};
      info_valla.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      VkDescriptorSetAllocateInfo reserva_desc{};
      reserva_desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      reserva_desc.descriptorPool = pool_descriptores_;
      reserva_desc.descriptorSetCount = 1;
      reserva_desc.pSetLayouts = &rec_.layout_descriptores;
      if (dfn_.vkAllocateCommandBuffers(device_, &reserva, &comandos[i].cmd) != VK_SUCCESS ||
          dfn_.vkCreateFence(device_, &info_valla, nullptr, &comandos[i].valla) != VK_SUCCESS ||
          dfn_.vkAllocateDescriptorSets(device_, &reserva_desc, &comandos[i].conjunto) != VK_SUCCESS) {
        return false;
      }
    }
    return true;
  }

  // Waits for a slot's previous use and starts recording it.
  bool Empezar(Comandos& c) {
    if (c.pendiente) {
      dfn_.vkWaitForFences(device_, 1, &c.valla, VK_TRUE, UINT64_MAX);
      dfn_.vkResetFences(device_, 1, &c.valla);
      c.pendiente = false;
    }
    dfn_.vkResetCommandPool(device_, c.pool, 0);
    VkCommandBufferBeginInfo inicio{};
    inicio.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    inicio.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    return dfn_.vkBeginCommandBuffer(c.cmd, &inicio) == VK_SUCCESS;
  }

  bool Enviar(Comandos& c) {
    if (dfn_.vkEndCommandBuffer(c.cmd) != VK_SUCCESS) {
      return false;
    }
    VkSubmitInfo envio{};
    envio.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    envio.commandBufferCount = 1;
    envio.pCommandBuffers = &c.cmd;
    if (EnviarACola(VK_NULL_HANDLE, 1, &envio, c.valla) != VK_SUCCESS) {
      return false;
    }
    c.pendiente = true;
    return true;
  }

  template <size_t N>
  void EsperarComandos(std::array<Comandos, N>& comandos) {
    for (Comandos& c : comandos) {
      if (c.pendiente) {
        dfn_.vkWaitForFences(device_, 1, &c.valla, VK_TRUE, UINT64_MAX);
        dfn_.vkResetFences(device_, 1, &c.valla);
        c.pendiente = false;
      }
    }
  }

  // A full-screen pass sampling 'vista' (with the bilinear shader: 1 to 1 here, so the exact texel).
  void Pasada(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb, VkPipeline pipeline, VkDescriptorSet conjunto,
              VkImageView vista, VkImageLayout layout, uint32_t ancho, uint32_t alto) {
    VkDescriptorImageInfo info_imagen{VK_NULL_HANDLE, vista, layout};
    VkWriteDescriptorSet escritura{};
    escritura.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    escritura.dstSet = conjunto;
    escritura.dstBinding = 0;
    escritura.descriptorCount = 1;
    escritura.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    escritura.pImageInfo = &info_imagen;
    dfn_.vkUpdateDescriptorSets(device_, 1, &escritura, 0, nullptr);
    VkRenderPassBeginInfo pase{};
    pase.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pase.renderPass = rp;
    pase.framebuffer = fb;
    pase.renderArea.extent = {ancho, alto};
    dfn_.vkCmdBeginRenderPass(cmd, &pase, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{0.0f, 0.0f, float(ancho), float(alto), 0.0f, 1.0f};
    dfn_.vkCmdSetViewport(cmd, 0, 1, &viewport);
    const VkRect2D tijera{{0, 0}, {ancho, alto}};
    dfn_.vkCmdSetScissor(cmd, 0, 1, &tijera);
    dfn_.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rec_.layout_pipeline, 0, 1, &conjunto, 0,
                                 nullptr);
    const float rectangulo[4] = {-1.0f, -1.0f, 2.0f, 2.0f};
    dfn_.vkCmdPushConstants(cmd, rec_.layout_pipeline, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(rectangulo),
                            rectangulo);
    struct {
      int32_t desplazamiento[2];
      float inverso[2];
    } bilineal = {{0, 0}, {1.0f / float(ancho), 1.0f / float(alto)}};
    dfn_.vkCmdPushConstants(cmd, rec_.layout_pipeline, VK_SHADER_STAGE_FRAGMENT_BIT, 16, sizeof(bilineal),
                            &bilineal);
    dfn_.vkCmdDraw(cmd, 4, 1, 0, 0);
    dfn_.vkCmdEndRenderPass(cmd);
  }

  void Barrera(VkCommandBuffer cmd, VkPipelineStageFlags de, VkAccessFlags acceso_de, VkPipelineStageFlags a,
               VkAccessFlags acceso_a) {
    VkMemoryBarrier barrera{};
    barrera.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrera.srcAccessMask = acceso_de;
    barrera.dstAccessMask = acceso_a;
    dfn_.vkCmdPipelineBarrier(cmd, de, a, 0, 1, &barrera, 0, nullptr, 0, nullptr);
  }

  void Convertir(uint32_t i) {
    k_conversion_ = (k_conversion_ + 1) % kComandosConversion;
    Comandos& c = conversion_[k_conversion_];
    if (!Empezar(c)) {
      throw std::runtime_error("no se pudo grabar la copia a la entrada de LSFG");
    }
    // The output pass wrote the frame; framegen (compute) and the pacing thread (fragment) read this input two
    // frames ago.
    Barrera(c.cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    Pasada(c.cmd, render_pass_lsfg_, framebuffers_entrada_[i], pipeline_conversion_, c.conjunto, cuadro_.vista,
           VulkanPresenter::kGuestOutputInternalLayout, ancho_, alto_);
    // framegen's barriers only cover compute shaders: the input written here, for its compute and for the pacing
    // thread's fragment shader.
    Barrera(c.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    if (!Enviar(c)) {
      throw std::runtime_error("no se pudo enviar la copia a la entrada de LSFG");
    }
  }

  bool CrearTamano(uint32_t ancho, uint32_t alto) {
    EsperarMostrador();
    EsperarComandos(conversion_);
    DestruirTamano();
    const auto antes = Reloj::now();
    try {
      LSFG_NFSMW::createContext({ancho, alto}, kFormatoLsfg);  // the first time also translates the shaders
      for (uint32_t i = 0; i < 2; ++i) {
        entradas_[i] = LSFG_NFSMW::input(i);
      }
    } catch (const std::exception& e) {
      REXLOG_WARN("[lsfg] no se pudo crear el contexto de {}x{}: {}", ancho, alto, e.what());
      return false;
    }
    // The frame image, in the output pass's format.
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VulkanPresenter::kGuestOutputFormat;
    info.extent = {ancho, alto, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!rex::ui::vulkan::util::CreateDedicatedAllocationImage(dispositivo_, info,
                                                               rex::ui::vulkan::util::MemoryPurpose::kDeviceLocal,
                                                               cuadro_.imagen, cuadro_.memoria)) {
      return false;
    }
    VkImageViewCreateInfo info_vista{};
    info_vista.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info_vista.image = cuadro_.imagen;
    info_vista.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info_vista.format = VulkanPresenter::kGuestOutputFormat;
    info_vista.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (dfn_.vkCreateImageView(device_, &info_vista, nullptr, &cuadro_.vista) != VK_SUCCESS) {
      return false;
    }
    // Framebuffers of both inputs, which go to GENERAL once (framegen is told).
    for (uint32_t i = 0; i < 2; ++i) {
      VkFramebufferCreateInfo info_fb{};
      info_fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      info_fb.renderPass = render_pass_lsfg_;
      info_fb.attachmentCount = 1;
      info_fb.pAttachments = &entradas_[i].view;
      info_fb.width = ancho;
      info_fb.height = alto;
      info_fb.layers = 1;
      if (dfn_.vkCreateFramebuffer(device_, &info_fb, nullptr, &framebuffers_entrada_[i]) != VK_SUCCESS) {
        return false;
      }
    }
    Comandos& c = conversion_[k_conversion_];
    if (!Empezar(c)) {
      return false;
    }
    VkImageMemoryBarrier barreras[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
      barreras[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barreras[i].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
      barreras[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barreras[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
      barreras[i].srcQueueFamilyIndex = barreras[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barreras[i].image = entradas_[i].image;
      barreras[i].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }
    dfn_.vkCmdPipelineBarrier(c.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                              0, nullptr, 0, nullptr, 2, barreras);
    if (!Enviar(c)) {
      return false;
    }
    try {
      LSFG_NFSMW::inputsAreGeneral();
    } catch (const std::exception&) {
      return false;
    }
    ancho_ = ancho;
    alto_ = alto;
    version_cuadro_ = 0xF5F6000000000000ull + (++generacion_);  // never a presenter image version
    primero_ = true;
    REXLOG_INFO("[lsfg] contexto de {}x{} listo en {:.0f} ms", ancho, alto,
                std::chrono::duration<double, std::milli>(Reloj::now() - antes).count());
    return true;
  }

  void DestruirTamano() {
    for (VkFramebuffer& fb : framebuffers_entrada_) {
      if (fb != VK_NULL_HANDLE) dfn_.vkDestroyFramebuffer(device_, fb, nullptr);
      fb = VK_NULL_HANDLE;
    }
    if (cuadro_.vista != VK_NULL_HANDLE) dfn_.vkDestroyImageView(device_, cuadro_.vista, nullptr);
    if (cuadro_.imagen != VK_NULL_HANDLE) dfn_.vkDestroyImage(device_, cuadro_.imagen, nullptr);
    if (cuadro_.memoria != VK_NULL_HANDLE) dfn_.vkFreeMemory(device_, cuadro_.memoria, nullptr);
    cuadro_ = ImagenPropia{};
    ancho_ = alto_ = 0;
  }

  VkPipeline CrearPipeline(VkRenderPass rp, VkShaderModule fs) {
    VkPipelineShaderStageCreateInfo etapas[2]{};
    etapas[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    etapas[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    etapas[0].module = rec_.vs;
    etapas[0].pName = "main";
    etapas[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    etapas[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    etapas[1].module = fs;
    etapas[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo entrada{};
    entrada.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo ensamblado{};
    ensamblado.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ensamblado.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineViewportStateCreateInfo vista{};
    vista.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vista.viewportCount = 1;
    vista.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterizado{};
    rasterizado.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizado.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizado.cullMode = VK_CULL_MODE_NONE;
    rasterizado.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizado.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo muestreo{};
    muestreo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    muestreo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState mezcla_adjunto{};
    mezcla_adjunto.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo mezcla{};
    mezcla.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    mezcla.attachmentCount = 1;
    mezcla.pAttachments = &mezcla_adjunto;
    const VkDynamicState dinamicos[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dinamico{};
    dinamico.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dinamico.dynamicStateCount = 2;
    dinamico.pDynamicStates = dinamicos;
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = etapas;
    info.pVertexInputState = &entrada;
    info.pInputAssemblyState = &ensamblado;
    info.pViewportState = &vista;
    info.pRasterizationState = &rasterizado;
    info.pMultisampleState = &muestreo;
    info.pColorBlendState = &mezcla;
    info.pDynamicState = &dinamico;
    info.layout = rec_.layout_pipeline;
    info.renderPass = rp;
    info.basePipelineIndex = -1;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (dfn_.vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return pipeline;
  }

  // ---- The pacing thread ----

  // Until the pacing thread has finished what it has (cut short: the real frame at once).
  void EsperarMostrador() {
    std::unique_lock<std::mutex> candado(m_);
    if (ocupado_ || hay_pendiente_) {
      prisa_ = true;
      cv_.notify_all();
      cv_.wait(candado, [this] { return (!ocupado_ && !hay_pendiente_) || salir_; });
    }
  }

  void Mostrador() {
    std::unique_lock<std::mutex> candado(m_);
    for (;;) {
      cv_.wait(candado, [this] { return hay_pendiente_ || salir_; });
      if (salir_) {
        return;
      }
      const Entrega e = std::move(pendiente_);
      hay_pendiente_ = false;
      ocupado_ = true;
      prisa_ = false;
      candado.unlock();
      // The generated ones, then the real one, one step apart: (multiplier) steps per interval.
      std::vector<VkImageView> cola = e.generadas;
      cola.push_back(e.real);
      const auto paso = std::chrono::duration<double, std::milli>(e.intervalo_ms / double(generadas_ + 1));
      for (size_t i = 0; i < cola.size(); ++i) {
        if (i > 0) {
          const auto cuando = e.llegada + std::chrono::duration_cast<Reloj::duration>(paso * double(i));
          candado.lock();
          cv_.wait_until(candado, cuando, [this] { return prisa_ || salir_; });
          const bool prisa = prisa_ || salir_;
          candado.unlock();
          if (prisa && i + 1 < cola.size()) {
            recortados_.fetch_add(1, std::memory_order_relaxed);
            i = cola.size() - 2;  // straight to the real one
            continue;
          }
        }
        Mostrar(e, cola[i]);
      }
      candado.lock();
      ocupado_ = false;
      cv_.notify_all();
    }
  }

  void Mostrar(const Entrega& e, VkImageView vista) {
    e.presentador->RefreshGuestOutput(
        e.ancho, e.alto, e.aspecto_x, e.aspecto_y, [&](rex::ui::Presenter::GuestOutputRefreshContext& base) {
          auto& contexto = static_cast<VulkanPresenter::VulkanGuestOutputRefreshContext&>(base);
          k_mostrar_ = (k_mostrar_ + 1) % kComandosMostrar;
          Comandos& c = mostrar_[k_mostrar_];
          VkFramebuffer fb = FramebufferMostrar(contexto, e.ancho, e.alto);
          if (fb == VK_NULL_HANDLE || !Empezar(c)) {
            return false;
          }
          // framegen's compute or the copy pass wrote the image; the presenter read this one (its paint).
          Barrera(c.cmd,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                  VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
          Pasada(c.cmd, rec_.render_pass_salida, fb, rec_.pipeline_bilineal, c.conjunto, vista,
                 VK_IMAGE_LAYOUT_GENERAL, e.ancho, e.alto);
          // framegen writes its outputs again (compute) after this has read them.
          dfn_.vkCmdPipelineBarrier(c.cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                    0, 0, nullptr, 0, nullptr, 0, nullptr);
          if (!Enviar(c)) {
            return false;
          }
          contexto.SetIs8bpc(true);
          return true;
        });
  }

  VkFramebuffer FramebufferMostrar(const VulkanPresenter::VulkanGuestOutputRefreshContext& contexto, uint32_t ancho,
                                   uint32_t alto) {
    const uint64_t version = contexto.image_version();
    for (const auto& f : framebuffers_mostrar_) {
      if (f.first == version) {
        return f.second;
      }
    }
    if (framebuffers_mostrar_.size() >= kFramebuffersMostrar) {
      EsperarComandos(mostrar_);  // a pending copy could still use it
      dfn_.vkDestroyFramebuffer(device_, framebuffers_mostrar_.front().second, nullptr);
      framebuffers_mostrar_.erase(framebuffers_mostrar_.begin());
    }
    VkImageView vista = contexto.image_view();
    VkFramebufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = rec_.render_pass_salida;
    info.attachmentCount = 1;
    info.pAttachments = &vista;
    info.width = ancho;
    info.height = alto;
    info.layers = 1;
    VkFramebuffer fb = VK_NULL_HANDLE;
    if (dfn_.vkCreateFramebuffer(device_, &info, nullptr, &fb) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    framebuffers_mostrar_.emplace_back(version, fb);
    return fb;
  }

  const VulkanDevice* dispositivo_;
  const VulkanDevice::Functions& dfn_;
  VkDevice device_;
  uint32_t familia_;
  RecursosSalida rec_;
  const uint32_t generadas_;

  VkRenderPass render_pass_lsfg_ = VK_NULL_HANDLE;
  VkShaderModule fs_bilineal_ = VK_NULL_HANDLE;
  VkPipeline pipeline_conversion_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_descriptores_ = VK_NULL_HANDLE;
  std::array<Comandos, kComandosConversion> conversion_{};  // ring thread
  std::array<Comandos, kComandosMostrar> mostrar_{};        // pacing thread
  uint32_t k_conversion_ = 0, k_mostrar_ = 0;
  std::vector<std::pair<uint64_t, VkFramebuffer>> framebuffers_mostrar_;  // pacing thread

  ImagenPropia cuadro_;
  std::array<LSFG_NFSMW::Image, 2> entradas_{};
  std::array<VkFramebuffer, 2> framebuffers_entrada_{};
  uint32_t ancho_ = 0, alto_ = 0;
  uint64_t version_cuadro_ = 0, generacion_ = 0;
  bool primero_ = true;
  bool fallido_ = false;
  Reloj::time_point ultima_{};
  double intervalo_ms_ = 33.3;
  uint64_t fotogramas_ = 0;

  std::thread hilo_;
  std::mutex m_;
  std::condition_variable cv_;
  Entrega pendiente_;
  bool hay_pendiente_ = false, ocupado_ = false, prisa_ = false, salir_ = false;
  std::atomic<uint64_t> recortados_{0};
};

}  // namespace

bool Activo() {
  static const bool activo = REXCVAR_GET(nfsmw_lsfg);
  return activo;
}

std::unique_ptr<LsfgVulkan> LsfgVulkan::Crear(const VulkanDevice* dispositivo, const RecursosSalida& recursos) {
  auto lsfg = std::make_unique<LsfgVulkanImpl>(dispositivo, recursos);
  if (!lsfg->Iniciar()) {
    REXLOG_WARN("[lsfg] sin generacion de fotogramas: se presenta como siempre");
    return nullptr;
  }
  return lsfg;
}

}  // namespace nfsmw::lsfg
