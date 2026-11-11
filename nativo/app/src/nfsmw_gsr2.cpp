// nfsmw - Snapdragon Game Super Resolution 2 in the native renderer (cvar nfsmw_gsr2)
//
// ===========================================================================
//  WHAT IT IS
//
//  SGSR 2 (github.com/SnapdragonStudios/snapdragon-gsr, BSD-3) is Qualcomm's temporal upscaler: it rebuilds the
//  frame at the screen's resolution from the current low-resolution frame and the previous upscaled one, moved
//  to where each pixel was. This is its 2-pass fragment shader variant, the one Qualcomm recommends for phones:
//    1. convert (render size): motion of each pixel, how much to trust the history (depth clip) and here also
//       the HUD mask (shaders/nfsmw_sgsr2_convertir.frag);
//    2. upscale (screen size): Lanczos from the current frame, clamped history, blend (nfsmw_sgsr2_escalar.frag).
//  The result goes through the usual output pass (gamma ramp and post-processing) into the presenter's image,
//  which then has the screen's size: the presenter only copies it.
//
//  WHAT THE GAME DOES NOT GIVE AND HOW IT IS MADE UP
//  - Motion vectors: the game draws none. SGSR 2 builds them from the depth and clipToPrevClip (previous view x
//    projection times the inverse of the current one) when there are none: that is the camera's motion. Whatever
//    moves on its own (the other cars, the traffic) gets only the camera's motion, and the history clamp has to
//    hide the difference: some trail is expected there.
//  - The camera: view 1's view x projection, read at the end of eView::Update (sub_8243EC28, block + 128; see
//    nfsmw_panoramico.cpp). One per frame.
//  - Jitter: the sub-pixel offset (Halton 2,3, 32 phases, like Qualcomm's sample) is added to the projection in
//    the sub_8243EB70 hook, before eView::Update multiplies it by the view, so every matrix the frame computes
//    afterwards carries it. Only for views drawn into the scene's target (the size of view 1's target): the
//    shadow maps, reflections and the 2D are not touched. The HUD is not jittered, so the upscale pass takes
//    its samples without jitter where the HUD mask says so.
//  - Depth: the depth target bound when the scene was resolved for the first time in the frame (the same render
//    target the frame comes from).
//  - HUD: the game copies the composited scene into a texture right before drawing the HUD (sub_82442478; the
//    raindrops read it). Where the final frame differs from that copy, it is HUD (or raindrops): no history there.
//
//  WHICH CAMERA GOES WITH WHICH SWAP
//  The preparer fills frame N's command list (sub_82445660, eView::Update inside) and the executor runs it and
//  calls VdSwap; the preparer can already be filling N+1 when VdSwap(N) runs. So the camera is queued when the
//  fill ends and VdSwap takes the oldest one, writes its number in the PM4_XE_SWAP packet (SDK patch,
//  xboxkrnl_video.cpp) and the ring thread finds it again by that number when it presents that Swap.
//
//  WITHOUT TESTING ON A PHONE (written 2026-11-11). What to look at first: the log lines "[gsr2]"; that the image
//  is not shaken by the jitter (if it is, nfsmw_gsr2_jitter=false); trails behind the cars; the HUD.
// ===========================================================================

#include "nfsmw_gsr2.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/vulkan/device.h>
#include <rex/ui/vulkan/util.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

REXCVAR_DEFINE_BOOL(nfsmw_gsr2, false, "NFSMW",
                    "Snapdragon GSR 2: el renderizador nativo (Vulkan) escala el juego al tamano de la pantalla con el "
                    "escalador temporal de Qualcomm (fotograma anterior + profundidad + movimiento de la camara). Solo "
                    "sirve con el juego a menos resolucion que la pantalla")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(nfsmw_gsr2_jitter, true, "NFSMW",
                    "Snapdragon GSR 2: desplaza la camara una fraccion de pixel cada fotograma (Halton 2,3) para que el "
                    "historial reuna mas detalle. false = sin desplazamiento (solo estabiliza)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(nfsmw_gsr2_mascara_hud, true, "NFSMW",
                    "Snapdragon GSR 2: sin historial donde el fotograma difiere de la escena compuesta antes del HUD "
                    "(el HUD y las gotas). false = todo con historial");
REXCVAR_DEFINE_DOUBLE(nfsmw_gsr2_umbral_hud, 0.008, "NFSMW",
                      "Snapdragon GSR 2: diferencia de color (0-1) a partir de la cual un pixel es HUD")
    .range(0.0, 1.0);

namespace rex::kernel::xboxkrnl {
extern uint32_t (*g_nfsmw_secuencia_swap)();  // SDK patch, xboxkrnl_video.cpp
}

namespace nfsmw::gsr2 {
namespace {

constexpr uint32_t kTablaPases = 0x82C3F590;  // nfsmw_panoramico.cpp
constexpr uint32_t kBytesPorPase = 384;
constexpr uint32_t kOffDestino = 368;
constexpr uint32_t kVistaEscena = 1;
constexpr uint32_t kFasesJitter = 32;
constexpr uint32_t kSwapsGuardados = 16;
constexpr size_t kColaMaxima = 4;

uint32_t Leer32(const uint8_t* base, uint32_t direccion) {
  uint32_t v = 0;
  std::memcpy(&v, base + direccion, sizeof(v));
  return __builtin_bswap32(v);
}
float LeerFloat(const uint8_t* base, uint32_t direccion) {
  const uint32_t v = Leer32(base, direccion);
  float f = 0.0f;
  std::memcpy(&f, &v, sizeof(f));
  return f;
}
void EscribirFloat(uint8_t* base, uint32_t direccion, float f) {
  uint32_t v = 0;
  std::memcpy(&v, &f, sizeof(v));
  v = __builtin_bswap32(v);
  std::memcpy(base + direccion, &v, sizeof(v));
}

// Size of a view's render target (640x480 if it has none), as eView::Update reads it.
void TamanoDestino(const uint8_t* base, uint32_t id, uint32_t& ancho, uint32_t& alto) {
  const uint32_t destino = Leer32(base, kTablaPases + kBytesPorPase * id + kOffDestino);
  ancho = destino ? Leer32(base, destino + 16) : 640;
  alto = destino ? Leer32(base, destino + 20) : 480;
}

constexpr float Halton(uint32_t indice, uint32_t base_numerica) {
  float resultado = 0.0f;
  float fraccion = 1.0f / float(base_numerica);
  while (indice > 0) {
    resultado += float(indice % base_numerica) * fraccion;
    indice /= base_numerica;
    fraccion /= float(base_numerica);
  }
  return resultado;
}

// ---- The frame being filled (preparer thread only) ----
struct Llenado {
  bool dentro = false;
  uint32_t numero = 0;
  float jitter_px[2] = {};   // this frame's phase, in pixels of the scene's target
  float jitter_ndc[2] = {};  // what was added to the projection (0 if nothing)
  Camara camara;
};
Llenado g_llenado;

// ---- From the preparer to VdSwap, and from VdSwap to the ring ----
std::mutex g_candado;
std::deque<Camara> g_cola;                                       // filled frames without a Swap yet
std::array<std::pair<uint32_t, Camara>, kSwapsGuardados> g_swaps{};  // by sequence number
uint32_t g_secuencia = 0;

// ---- Ring thread ----
Camara g_camara_swap;
std::atomic<bool> g_escalando{false};  // the last presented frame went through GSR 2 (read by the preparer)

uint32_t AlHacerSwap() {
  std::lock_guard<std::mutex> candado(g_candado);
  uint32_t secuencia = ++g_secuencia;
  if (secuencia == 0) {
    secuencia = ++g_secuencia;  // 0 means "none"
  }
  Camara camara;
  if (!g_cola.empty()) {
    camara = g_cola.front();
    g_cola.pop_front();
  }
  g_swaps[secuencia % kSwapsGuardados] = {secuencia, camara};
  return secuencia;
}

}  // namespace

bool Activo() {
  static const bool activo = REXCVAR_GET(nfsmw_gsr2);
  return activo;
}

void EmpezarLlenado() {
  if (!Activo()) {
    return;
  }
  // VdSwap asks for the sequence number from now on (the first fill comes before the first Swap that matters).
  if (rex::kernel::xboxkrnl::g_nfsmw_secuencia_swap == nullptr) {
    rex::kernel::xboxkrnl::g_nfsmw_secuencia_swap = &AlHacerSwap;
    REXLOG_INFO("[gsr2] activo: jitter {}, mascara del HUD {}", REXCVAR_GET(nfsmw_gsr2_jitter) ? "si" : "no",
                REXCVAR_GET(nfsmw_gsr2_mascara_hud) ? "si" : "no");
  }
  Llenado& l = g_llenado;
  l.dentro = true;
  ++l.numero;
  const uint32_t fase = l.numero % kFasesJitter;
  if (REXCVAR_GET(nfsmw_gsr2_jitter) && g_escalando.load(std::memory_order_relaxed)) {
    l.jitter_px[0] = Halton(fase + 1, 2) - 0.5f;
    l.jitter_px[1] = Halton(fase + 1, 3) - 0.5f;
  } else {
    l.jitter_px[0] = l.jitter_px[1] = 0.0f;
  }
  l.jitter_ndc[0] = l.jitter_ndc[1] = 0.0f;
  l.camara = Camara{};
}

void TerminarLlenado() {
  if (!Activo()) {
    return;
  }
  Llenado& l = g_llenado;
  l.dentro = false;
  Camara camara = l.camara;
  if (camara.valida) {
    camara.jitter_ndc[0] = l.jitter_ndc[0];
    camara.jitter_ndc[1] = l.jitter_ndc[1];
  }
  std::lock_guard<std::mutex> candado(g_candado);
  g_cola.push_back(camara);
  while (g_cola.size() > kColaMaxima) {
    g_cola.pop_front();
  }
}

void AjustarProyeccion(uint8_t* base, uint32_t vista, uint32_t proyeccion) {
  Llenado& l = g_llenado;
  if (!Activo() || !l.dentro || (l.jitter_px[0] == 0.0f && l.jitter_px[1] == 0.0f)) {
    return;
  }
  const uint32_t id = Leer32(base, vista + 4);
  uint32_t ancho = 0, alto = 0, ancho_escena = 0, alto_escena = 0;
  TamanoDestino(base, id, ancho, alto);
  TamanoDestino(base, kVistaEscena, ancho_escena, alto_escena);
  if (ancho == 0 || alto == 0 || ancho != ancho_escena || alto != alto_escena) {
    return;  // not drawn into the scene's target
  }
  // Content moved by jitter_px pixels (y down): x' = x + jx * w, y' = y + jy * w in clip space, that is, column 0
  // (and 1) of the projection plus jx (jy) times column 3, for row vectors.
  const float jx = 2.0f * l.jitter_px[0] / float(ancho);
  const float jy = -2.0f * l.jitter_px[1] / float(alto);
  for (uint32_t fila = 0; fila < 4; ++fila) {
    const uint32_t f = proyeccion + fila * 16;
    const float w = LeerFloat(base, f + 12);
    if (w == 0.0f) {
      continue;
    }
    EscribirFloat(base, f + 0, LeerFloat(base, f + 0) + jx * w);
    EscribirFloat(base, f + 4, LeerFloat(base, f + 4) + jy * w);
  }
  l.jitter_ndc[0] = jx;
  l.jitter_ndc[1] = jy;
}

void TrasUpdate(uint8_t* base, uint32_t vista, uint32_t bloque) {
  Llenado& l = g_llenado;
  if (!Activo() || !l.dentro || bloque == 0 || Leer32(base, vista + 4) != kVistaEscena) {
    return;
  }
  Camara c;
  bool finita = true;
  for (uint32_t i = 0; i < 16; ++i) {
    c.vp[i] = double(LeerFloat(base, bloque + 128 + i * 4));
    finita = finita && std::isfinite(c.vp[i]);
  }
  // Without the jitter: column 0 (1) minus jx (jy) times column 3.
  for (uint32_t fila = 0; fila < 4; ++fila) {
    c.vp[fila * 4 + 0] -= double(l.jitter_ndc[0]) * c.vp[fila * 4 + 3];
    c.vp[fila * 4 + 1] -= double(l.jitter_ndc[1]) * c.vp[fila * 4 + 3];
  }
  const uint32_t p = bloque + 64;
  const double p00 = LeerFloat(base, p + 0);
  const double p22 = LeerFloat(base, p + 40), p23 = LeerFloat(base, p + 44);
  const double p32 = LeerFloat(base, p + 56), p33 = LeerFloat(base, p + 60);
  c.tan_mitad_horizontal = p00 != 0.0 ? float(1.0 / std::fabs(p00)) : 1.0f;
  // Depth of a point 1 and 1000 units ahead: if it goes down with distance, it is reversed.
  const double s = p23 >= 0.0 ? 1.0 : -1.0;
  const auto ndc_z = [&](double z) {
    const double w = z * p23 + p33;
    return w != 0.0 ? (z * p22 + p32) / w : 0.0;
  };
  c.invertida = ndc_z(1000.0 * s) < ndc_z(1.0 * s);
  c.valida = finita && p23 != 0.0;
  l.camara = c;
  static std::atomic<uint32_t> avisos{0};
  if (avisos.fetch_add(1, std::memory_order_relaxed) < 2) {
    REXLOG_INFO("[gsr2] camara de la vista 1: tan(fov/2) {:.4f}, profundidad {}, P22 {:.5f} P23 {:.3f} P32 {:.5f} "
                "P33 {:.3f}, jitter {:.5f},{:.5f} NDC",
                c.tan_mitad_horizontal, c.invertida ? "invertida (1 cerca)" : "normal (0 cerca)", p22, p23, p32, p33,
                l.jitter_ndc[0], l.jitter_ndc[1]);
  }
}

void SwapEnAnillo(uint32_t secuencia) {
  if (secuencia == 0) {
    g_camara_swap = Camara{};
    return;
  }
  std::lock_guard<std::mutex> candado(g_candado);
  const auto& entrada = g_swaps[secuencia % kSwapsGuardados];
  g_camara_swap = entrada.first == secuencia ? entrada.second : Camara{};
}

const Camara& CamaraDelSwap() { return g_camara_swap; }

void AnotarUso(bool escalado) {
  if (g_escalando.exchange(escalado, std::memory_order_relaxed) != escalado) {
    REXLOG_INFO("[gsr2] {}", escalado ? "escalando: la camara se desplaza (jitter) desde el siguiente fotograma"
                                      : "sin escalar este fotograma: sin desplazamiento de la camara");
  }
}

// =====================================================================================================
//  The passes
// =====================================================================================================
namespace {

namespace shaders {
#include "shaders/nfsmw_sgsr2_convertir_ps.h"
#include "shaders/nfsmw_sgsr2_escalar_ps.h"
}  // namespace shaders

constexpr VkFormat kFormato = VK_FORMAT_R16G16B16A16_SFLOAT;

// The uniform block of both shaders (std140).
struct ParametrosGpu {
  float clip_a_previo[4][4];
  float render[2];
  float salida[2];
  float render_rcp[2];
  float salida_rcp[2];
  float jitter[2];
  float escala[2];
  float profundidad_escala[2];
  float profundidad_desplazamiento[2];
  float profundidad_rcp[2];
  float relleno0[2];
  float fov;
  float min_lerp;
  float reset;
  float invertida;
  float umbral;
  float hay_escena;
  float escena_rb;
  float relleno1;
};
static_assert(sizeof(ParametrosGpu) == 176, "the shaders' std140 block");

// a x b, 4x4 row-major.
void Multiplicar(const double* a, const double* b, double* r) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      double s = 0.0;
      for (int k = 0; k < 4; ++k) {
        s += a[i * 4 + k] * b[k * 4 + j];
      }
      r[i * 4 + j] = s;
    }
  }
}

bool Invertir(const double* m, double* r) {
  double inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
           m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
           m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
           m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
            m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
           m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
           m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
           m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
            m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
           m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
           m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
            m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
            m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
           m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
           m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
            m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
            m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (det == 0.0 || !std::isfinite(det)) {
    return false;
  }
  for (int i = 0; i < 16; ++i) {
    r[i] = inv[i] / det;
  }
  return true;
}

using rex::ui::vulkan::VulkanDevice;

class Gsr2VulkanImpl final : public Gsr2Vulkan {
 public:
  Gsr2VulkanImpl(const VulkanDevice* dispositivo, VkShaderModule vs, VkSampler lineal, uint32_t ranuras,
                 void (*esperar)(void*), void* esperar_dato)
      : dispositivo_(dispositivo),
        dfn_(dispositivo->functions()),
        device_(dispositivo->device()),
        vs_(vs),
        lineal_(lineal),
        esperar_(esperar),
        esperar_dato_(esperar_dato),
        ranuras_(ranuras) {}

  ~Gsr2VulkanImpl() override {
    if (esperar_) {
      esperar_(esperar_dato_);
    }
    DestruirImagenes();
    for (Ranura& r : porRanura_) {
      if (r.vista_profundidad != VK_NULL_HANDLE) dfn_.vkDestroyImageView(device_, r.vista_profundidad, nullptr);
      if (r.bufer != VK_NULL_HANDLE) dfn_.vkDestroyBuffer(device_, r.bufer, nullptr);
      if (r.memoria != VK_NULL_HANDLE) dfn_.vkFreeMemory(device_, r.memoria, nullptr);
    }
    for (VkPipeline p : {pipeline_convertir_, pipeline_escalar_}) {
      if (p != VK_NULL_HANDLE) dfn_.vkDestroyPipeline(device_, p, nullptr);
    }
    for (VkShaderModule m : {fs_convertir_, fs_escalar_}) {
      if (m != VK_NULL_HANDLE) dfn_.vkDestroyShaderModule(device_, m, nullptr);
    }
    if (render_pass_ != VK_NULL_HANDLE) dfn_.vkDestroyRenderPass(device_, render_pass_, nullptr);
    if (pool_ != VK_NULL_HANDLE) dfn_.vkDestroyDescriptorPool(device_, pool_, nullptr);
    if (layout_pipeline_ != VK_NULL_HANDLE) dfn_.vkDestroyPipelineLayout(device_, layout_pipeline_, nullptr);
    if (layout_conjunto_ != VK_NULL_HANDLE) dfn_.vkDestroyDescriptorSetLayout(device_, layout_conjunto_, nullptr);
    if (cercano_ != VK_NULL_HANDLE) dfn_.vkDestroySampler(device_, cercano_, nullptr);
  }

  bool Iniciar() {
    VkDescriptorSetLayoutBinding enlaces[5]{};
    enlaces[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    for (uint32_t i = 1; i <= 3; ++i) {
      enlaces[i] = {i, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    }
    // The sampler per set: nearest for the convert pass (textureGather of a depth format, which need not support
    // linear filtering), linear for the upscale pass (history and motion with textureLod).
    enlaces[4] = {4, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkSamplerCreateInfo info_cercano{};
    info_cercano.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info_cercano.magFilter = VK_FILTER_NEAREST;
    info_cercano.minFilter = VK_FILTER_NEAREST;
    info_cercano.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info_cercano.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info_cercano.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info_cercano.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (dfn_.vkCreateSampler(device_, &info_cercano, nullptr, &cercano_) != VK_SUCCESS) {
      return false;
    }
    VkDescriptorSetLayoutCreateInfo info_layout{};
    info_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info_layout.bindingCount = 5;
    info_layout.pBindings = enlaces;
    if (dfn_.vkCreateDescriptorSetLayout(device_, &info_layout, nullptr, &layout_conjunto_) != VK_SUCCESS) {
      return false;
    }
    VkPushConstantRange rango{VK_SHADER_STAGE_VERTEX_BIT, 0, 16};  // GuestOutputPaintRectangleConstants
    VkPipelineLayoutCreateInfo info_pl{};
    info_pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info_pl.setLayoutCount = 1;
    info_pl.pSetLayouts = &layout_conjunto_;
    info_pl.pushConstantRangeCount = 1;
    info_pl.pPushConstantRanges = &rango;
    if (dfn_.vkCreatePipelineLayout(device_, &info_pl, nullptr, &layout_pipeline_) != VK_SUCCESS) {
      return false;
    }
    const uint32_t conjuntos = ranuras_ * 2;
    VkDescriptorPoolSize tamanos[3] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, conjuntos},
                                       {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, conjuntos * 3},
                                       {VK_DESCRIPTOR_TYPE_SAMPLER, conjuntos}};
    VkDescriptorPoolCreateInfo info_pool{};
    info_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info_pool.maxSets = conjuntos;
    info_pool.poolSizeCount = 3;
    info_pool.pPoolSizes = tamanos;
    if (dfn_.vkCreateDescriptorPool(device_, &info_pool, nullptr, &pool_) != VK_SUCCESS) {
      return false;
    }
    porRanura_.resize(ranuras_);
    for (Ranura& r : porRanura_) {
      VkDescriptorSetLayout layouts[2] = {layout_conjunto_, layout_conjunto_};
      VkDescriptorSetAllocateInfo reserva{};
      reserva.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      reserva.descriptorPool = pool_;
      reserva.descriptorSetCount = 2;
      reserva.pSetLayouts = layouts;
      VkDescriptorSet sets[2];
      if (dfn_.vkAllocateDescriptorSets(device_, &reserva, sets) != VK_SUCCESS) {
        return false;
      }
      r.conjunto_convertir = sets[0];
      r.conjunto_escalar = sets[1];
      VkDescriptorImageInfo muestreadores[2] = {{cercano_, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED},
                                                {lineal_, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED}};
      VkWriteDescriptorSet escrituras[2]{};
      for (uint32_t i = 0; i < 2; ++i) {
        escrituras[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        escrituras[i].dstSet = sets[i];
        escrituras[i].dstBinding = 4;
        escrituras[i].descriptorCount = 1;
        escrituras[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        escrituras[i].pImageInfo = &muestreadores[i];
      }
      dfn_.vkUpdateDescriptorSets(device_, 2, escrituras, 0, nullptr);
      void* mapeado = nullptr;
      if (!rex::ui::vulkan::util::CreateDedicatedAllocationBuffer(
              dispositivo_, sizeof(ParametrosGpu), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              rex::ui::vulkan::util::MemoryPurpose::kUpload, r.bufer, r.memoria, &r.tipo_memoria) ||
          dfn_.vkMapMemory(device_, r.memoria, 0, VK_WHOLE_SIZE, 0, &mapeado) != VK_SUCCESS) {
        return false;
      }
      r.datos = mapeado;
    }
    fs_convertir_ = rex::ui::vulkan::util::CreateShaderModule(dispositivo_, shaders::nfsmw_sgsr2_convertir_ps,
                                                              sizeof(shaders::nfsmw_sgsr2_convertir_ps));
    fs_escalar_ = rex::ui::vulkan::util::CreateShaderModule(dispositivo_, shaders::nfsmw_sgsr2_escalar_ps,
                                                            sizeof(shaders::nfsmw_sgsr2_escalar_ps));
    if (fs_convertir_ == VK_NULL_HANDLE || fs_escalar_ == VK_NULL_HANDLE) {
      return false;
    }
    // Every image stays in GENERAL, like the renderer's.
    VkAttachmentDescription adjunto{};
    adjunto.format = kFormato;
    adjunto.samples = VK_SAMPLE_COUNT_1_BIT;
    adjunto.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adjunto.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    adjunto.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    adjunto.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    adjunto.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
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
    if (dfn_.vkCreateRenderPass(device_, &info_rp, nullptr, &render_pass_) != VK_SUCCESS) {
      return false;
    }
    pipeline_convertir_ = CrearPipeline(fs_convertir_);
    pipeline_escalar_ = CrearPipeline(fs_escalar_);
    return pipeline_convertir_ != VK_NULL_HANDLE && pipeline_escalar_ != VK_NULL_HANDLE;
  }

  VkImageView Grabar(VkCommandBuffer cmd, uint32_t ranura, const EntradasVulkan& e, uint32_t ancho_salida,
                     uint32_t alto_salida, const Camara& camara) override {
    if (ranura >= porRanura_.size() || e.color == VK_NULL_HANDLE || e.profundidad == VK_NULL_HANDLE || !e.ancho ||
        !e.alto || !e.profundidad_ancho || !e.profundidad_alto || !ancho_salida || !alto_salida) {
      return VK_NULL_HANDLE;
    }
    bool reset = false;
    if (ancho_render_ != e.ancho || alto_render_ != e.alto || ancho_salida_ != ancho_salida ||
        alto_salida_ != alto_salida) {
      if (!CrearImagenes(e.ancho, e.alto, ancho_salida, alto_salida)) {
        return VK_NULL_HANDLE;
      }
      reset = true;
    }
    Ranura& r = porRanura_[ranura];

    // Depth-only view of the depth target (the slot's previous one is no longer in use).
    if (r.vista_profundidad != VK_NULL_HANDLE) {
      dfn_.vkDestroyImageView(device_, r.vista_profundidad, nullptr);
      r.vista_profundidad = VK_NULL_HANDLE;
    }
    VkImageViewCreateInfo info_vista{};
    info_vista.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info_vista.image = e.profundidad;
    info_vista.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info_vista.format = e.formato_profundidad;
    info_vista.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    if (dfn_.vkCreateImageView(device_, &info_vista, nullptr, &r.vista_profundidad) != VK_SUCCESS) {
      r.vista_profundidad = VK_NULL_HANDLE;
      return VK_NULL_HANDLE;
    }

    // The parameters.
    ParametrosGpu p{};
    reset = reset || !camara.valida || !anterior_.valida;
    double m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    if (!reset) {
      double inversa[16];
      if (Invertir(camara.vp, inversa)) {
        Multiplicar(inversa, anterior_.vp, m);  // current clip -> world -> previous clip (row vectors)
        // A camera cut: the centre of the screen at mid depth lands far away (or behind) in the previous frame.
        const double w = 0.5 * m[11] + m[15];
        const double x = (0.5 * m[8] + m[12]) / w, y = (0.5 * m[9] + m[13]) / w;
        if (!(w > 0.0) || std::fabs(x) > 0.6 || std::fabs(y) > 0.6) {
          reset = true;
          ++cortes_;
        }
      } else {
        reset = true;
      }
    }
    if (reset) {
      std::fill(std::begin(m), std::end(m), 0.0);
      m[0] = m[5] = m[10] = m[15] = 1.0;
    }
    double diferencia = 0.0;
    for (int i = 0; i < 16; ++i) {
      p.clip_a_previo[i / 4][i % 4] = float(m[i]);
      diferencia += std::fabs(camara.vp[i] - anterior_.vp[i]);
    }
    quieta_ = (!reset && diferencia < 1e-5) ? quieta_ + 1 : 0;
    p.render[0] = float(e.ancho);
    p.render[1] = float(e.alto);
    p.salida[0] = float(ancho_salida);
    p.salida[1] = float(alto_salida);
    p.render_rcp[0] = 1.0f / p.render[0];
    p.render_rcp[1] = 1.0f / p.render[1];
    p.salida_rcp[0] = 1.0f / p.salida[0];
    p.salida_rcp[1] = 1.0f / p.salida[1];
    // NDC (y up) -> render pixels (y down).
    p.jitter[0] = camara.valida ? camara.jitter_ndc[0] * p.render[0] * 0.5f : 0.0f;
    p.jitter[1] = camara.valida ? -camara.jitter_ndc[1] * p.render[1] * 0.5f : 0.0f;
    p.escala[0] = p.salida[0] / p.render[0];
    p.escala[1] = std::min(20.0f, std::pow((p.salida[0] * p.salida[1]) / (p.render[0] * p.render[1]), 3.0f));
    const float pw = float(e.profundidad_ancho), ph = float(e.profundidad_alto);
    p.profundidad_escala[0] = (e.rect_profundidad[2] - e.rect_profundidad[0]) / pw;
    p.profundidad_escala[1] = (e.rect_profundidad[3] - e.rect_profundidad[1]) / ph;
    p.profundidad_desplazamiento[0] = e.rect_profundidad[0] / pw;
    p.profundidad_desplazamiento[1] = e.rect_profundidad[1] / ph;
    p.profundidad_rcp[0] = 1.0f / pw;
    p.profundidad_rcp[1] = 1.0f / ph;
    p.fov = camara.tan_mitad_horizontal;
    p.min_lerp = quieta_ > 5 ? 0.3f : 0.0f;
    p.reset = reset ? 1.0f : 0.0f;
    p.invertida = camara.invertida ? 1.0f : 0.0f;
    p.umbral = float(REXCVAR_GET(nfsmw_gsr2_umbral_hud));
    const bool mascara = e.escena != VK_NULL_HANDLE && REXCVAR_GET(nfsmw_gsr2_mascara_hud);
    p.hay_escena = mascara ? 1.0f : 0.0f;
    p.escena_rb = e.escena_rb ? 1.0f : 0.0f;
    std::memcpy(r.datos, &p, sizeof(p));
    rex::ui::vulkan::util::FlushMappedMemoryRange(dispositivo_, r.memoria, r.tipo_memoria);
    if (camara.valida) {
      anterior_ = camara;
    } else {
      anterior_.valida = false;
    }

    // The descriptors.
    const uint32_t actual = historia_actual_ ^ 1u;
    VkDescriptorBufferInfo info_bufer{r.bufer, 0, sizeof(ParametrosGpu)};
    VkDescriptorImageInfo imagenes_c[3] = {{VK_NULL_HANDLE, r.vista_profundidad, VK_IMAGE_LAYOUT_GENERAL},
                                           {VK_NULL_HANDLE, e.color, VK_IMAGE_LAYOUT_GENERAL},
                                           {VK_NULL_HANDLE, mascara ? e.escena : e.color, VK_IMAGE_LAYOUT_GENERAL}};
    VkDescriptorImageInfo imagenes_e[3] = {{VK_NULL_HANDLE, historia_[historia_actual_].vista, VK_IMAGE_LAYOUT_GENERAL},
                                           {VK_NULL_HANDLE, movimiento_.vista, VK_IMAGE_LAYOUT_GENERAL},
                                           {VK_NULL_HANDLE, e.color, VK_IMAGE_LAYOUT_GENERAL}};
    VkWriteDescriptorSet escrituras[4]{};
    for (uint32_t i = 0; i < 4; ++i) {
      escrituras[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    }
    escrituras[0].dstSet = r.conjunto_convertir;
    escrituras[0].dstBinding = 0;
    escrituras[0].descriptorCount = 1;
    escrituras[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    escrituras[0].pBufferInfo = &info_bufer;
    escrituras[1].dstSet = r.conjunto_convertir;
    escrituras[1].dstBinding = 1;
    escrituras[1].descriptorCount = 3;
    escrituras[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    escrituras[1].pImageInfo = imagenes_c;
    escrituras[2] = escrituras[0];
    escrituras[2].dstSet = r.conjunto_escalar;
    escrituras[3] = escrituras[1];
    escrituras[3].dstSet = r.conjunto_escalar;
    escrituras[3].pImageInfo = imagenes_e;
    dfn_.vkUpdateDescriptorSets(device_, 4, escrituras, 0, nullptr);

    // What the frame's work wrote (color, depth, the copies) and the previous output (the history) are read now.
    VkMemoryBarrier antes{};
    antes.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    antes.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                          VK_ACCESS_TRANSFER_WRITE_BIT;
    antes.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dfn_.vkCmdPipelineBarrier(cmd,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                              1, &antes, 0, nullptr, 0, nullptr);
    if (!iniciadas_) {
      // New images: GENERAL and cleared, so the first history read is not garbage (NaN would survive reset).
      std::array<VkImageMemoryBarrier, 3> barreras{};
      Imagen* imagenes[3] = {&movimiento_, &historia_[0], &historia_[1]};
      for (uint32_t i = 0; i < 3; ++i) {
        barreras[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barreras[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barreras[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barreras[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barreras[i].srcQueueFamilyIndex = barreras[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barreras[i].image = imagenes[i]->imagen;
        barreras[i].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      }
      dfn_.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                nullptr, 0, nullptr, 3, barreras.data());
      const VkClearColorValue negro{};
      const VkImageSubresourceRange rango{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      for (Imagen* imagen : imagenes) {
        dfn_.vkCmdClearColorImage(cmd, imagen->imagen, VK_IMAGE_LAYOUT_GENERAL, &negro, 1, &rango);
      }
      VkMemoryBarrier tras_borrar{};
      tras_borrar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      tras_borrar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      tras_borrar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      dfn_.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                0, 1, &tras_borrar, 0, nullptr, 0, nullptr);
      iniciadas_ = true;
    }

    // 1. Convert, at the render size.
    Pasada(cmd, movimiento_, e.ancho, e.alto, pipeline_convertir_, r.conjunto_convertir);
    VkMemoryBarrier entre{};
    entre.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    entre.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    entre.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dfn_.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              0, 1, &entre, 0, nullptr, 0, nullptr);
    // 2. Upscale, at the screen size, into the other history image.
    Pasada(cmd, historia_[actual], ancho_salida, alto_salida, pipeline_escalar_, r.conjunto_escalar);
    dfn_.vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              0, 1, &entre, 0, nullptr, 0, nullptr);
    historia_actual_ = actual;

    ++fotogramas_;
    resets_ += reset ? 1 : 0;
    if (fotogramas_ == 1 || fotogramas_ % 1800 == 0) {
      REXLOG_INFO("[gsr2] {} fotogramas de {}x{} a {}x{}: {} sin historial ({} cortes de camara); profundidad {}x{} "
                  "desde {:.0f},{:.0f}-{:.0f},{:.0f}; mascara del HUD {}",
                  fotogramas_, e.ancho, e.alto, ancho_salida, alto_salida, resets_, cortes_, e.profundidad_ancho,
                  e.profundidad_alto, e.rect_profundidad[0], e.rect_profundidad[1], e.rect_profundidad[2],
                  e.rect_profundidad[3], mascara ? "si" : "no (sin la copia de la escena)");
    }
    return historia_[actual].vista;
  }

 private:
  struct Imagen {
    VkImage imagen = VK_NULL_HANDLE;
    VkDeviceMemory memoria = VK_NULL_HANDLE;
    VkImageView vista = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
  };
  struct Ranura {
    VkDescriptorSet conjunto_convertir = VK_NULL_HANDLE;
    VkDescriptorSet conjunto_escalar = VK_NULL_HANDLE;
    VkBuffer bufer = VK_NULL_HANDLE;
    VkDeviceMemory memoria = VK_NULL_HANDLE;
    uint32_t tipo_memoria = 0;
    void* datos = nullptr;
    VkImageView vista_profundidad = VK_NULL_HANDLE;
  };

  void Pasada(VkCommandBuffer cmd, const Imagen& destino, uint32_t ancho, uint32_t alto, VkPipeline pipeline,
              VkDescriptorSet conjunto) {
    VkRenderPassBeginInfo pase{};
    pase.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pase.renderPass = render_pass_;
    pase.framebuffer = destino.framebuffer;
    pase.renderArea.extent = {ancho, alto};
    dfn_.vkCmdBeginRenderPass(cmd, &pase, VK_SUBPASS_CONTENTS_INLINE);
    const VkViewport viewport{0.0f, 0.0f, float(ancho), float(alto), 0.0f, 1.0f};
    dfn_.vkCmdSetViewport(cmd, 0, 1, &viewport);
    const VkRect2D tijera{{0, 0}, {ancho, alto}};
    dfn_.vkCmdSetScissor(cmd, 0, 1, &tijera);
    dfn_.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, 0, 1, &conjunto, 0, nullptr);
    const float rectangulo[4] = {-1.0f, -1.0f, 2.0f, 2.0f};  // x, y, width, height in NDC
    dfn_.vkCmdPushConstants(cmd, layout_pipeline_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(rectangulo), rectangulo);
    dfn_.vkCmdDraw(cmd, 4, 1, 0, 0);
    dfn_.vkCmdEndRenderPass(cmd);
  }

  bool CrearImagen(Imagen& imagen, uint32_t ancho, uint32_t alto) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = kFormato;
    info.extent = {ancho, alto, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!rex::ui::vulkan::util::CreateDedicatedAllocationImage(
            dispositivo_, info, rex::ui::vulkan::util::MemoryPurpose::kDeviceLocal, imagen.imagen, imagen.memoria)) {
      return false;
    }
    VkImageViewCreateInfo info_vista{};
    info_vista.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info_vista.image = imagen.imagen;
    info_vista.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info_vista.format = kFormato;
    info_vista.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (dfn_.vkCreateImageView(device_, &info_vista, nullptr, &imagen.vista) != VK_SUCCESS) {
      return false;
    }
    VkFramebufferCreateInfo info_fb{};
    info_fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info_fb.renderPass = render_pass_;
    info_fb.attachmentCount = 1;
    info_fb.pAttachments = &imagen.vista;
    info_fb.width = ancho;
    info_fb.height = alto;
    info_fb.layers = 1;
    return dfn_.vkCreateFramebuffer(device_, &info_fb, nullptr, &imagen.framebuffer) == VK_SUCCESS;
  }

  void DestruirImagen(Imagen& imagen) {
    if (imagen.framebuffer != VK_NULL_HANDLE) dfn_.vkDestroyFramebuffer(device_, imagen.framebuffer, nullptr);
    if (imagen.vista != VK_NULL_HANDLE) dfn_.vkDestroyImageView(device_, imagen.vista, nullptr);
    if (imagen.imagen != VK_NULL_HANDLE) dfn_.vkDestroyImage(device_, imagen.imagen, nullptr);
    if (imagen.memoria != VK_NULL_HANDLE) dfn_.vkFreeMemory(device_, imagen.memoria, nullptr);
    imagen = Imagen{};
  }

  void DestruirImagenes() {
    DestruirImagen(movimiento_);
    DestruirImagen(historia_[0]);
    DestruirImagen(historia_[1]);
    ancho_render_ = alto_render_ = ancho_salida_ = alto_salida_ = 0;
  }

  bool CrearImagenes(uint32_t ancho, uint32_t alto, uint32_t ancho_salida, uint32_t alto_salida) {
    if (movimiento_.imagen != VK_NULL_HANDLE && esperar_) {
      esperar_(esperar_dato_);  // the old ones may still be in a submitted output
    }
    DestruirImagenes();
    iniciadas_ = false;
    if (!CrearImagen(movimiento_, ancho, alto) || !CrearImagen(historia_[0], ancho_salida, alto_salida) ||
        !CrearImagen(historia_[1], ancho_salida, alto_salida)) {
      REXLOG_ERROR("[gsr2] no se pudieron crear las imagenes ({}x{} y 2 de {}x{})", ancho, alto, ancho_salida,
                   alto_salida);
      DestruirImagenes();
      return false;
    }
    ancho_render_ = ancho;
    alto_render_ = alto;
    ancho_salida_ = ancho_salida;
    alto_salida_ = alto_salida;
    REXLOG_INFO("[gsr2] imagenes: movimiento {}x{}, historial 2 de {}x{}", ancho, alto, ancho_salida, alto_salida);
    return true;
  }

  VkPipeline CrearPipeline(VkShaderModule fs) {
    VkPipelineShaderStageCreateInfo etapas[2]{};
    etapas[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    etapas[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    etapas[0].module = vs_;
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
    info.layout = layout_pipeline_;
    info.renderPass = render_pass_;
    info.basePipelineIndex = -1;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (dfn_.vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return pipeline;
  }

  const VulkanDevice* dispositivo_;
  const VulkanDevice::Functions& dfn_;
  VkDevice device_;
  VkShaderModule vs_;
  VkSampler lineal_;
  void (*esperar_)(void*);
  void* esperar_dato_;
  uint32_t ranuras_;

  VkSampler cercano_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout layout_conjunto_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_pipeline_ = VK_NULL_HANDLE;
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  VkShaderModule fs_convertir_ = VK_NULL_HANDLE;
  VkShaderModule fs_escalar_ = VK_NULL_HANDLE;
  VkPipeline pipeline_convertir_ = VK_NULL_HANDLE;
  VkPipeline pipeline_escalar_ = VK_NULL_HANDLE;
  std::vector<Ranura> porRanura_;

  Imagen movimiento_;
  Imagen historia_[2];
  uint32_t historia_actual_ = 0;  // the one written last
  bool iniciadas_ = false;
  uint32_t ancho_render_ = 0, alto_render_ = 0, ancho_salida_ = 0, alto_salida_ = 0;

  Camara anterior_;
  uint32_t quieta_ = 0;
  uint64_t fotogramas_ = 0, resets_ = 0, cortes_ = 0;
};

}  // namespace

std::unique_ptr<Gsr2Vulkan> Gsr2Vulkan::Crear(const VulkanDevice* dispositivo, VkShaderModule vs, VkSampler lineal,
                                              uint32_t ranuras, void (*esperar)(void*), void* esperar_dato) {
  auto gsr2 = std::make_unique<Gsr2VulkanImpl>(dispositivo, vs, lineal, ranuras, esperar, esperar_dato);
  if (!gsr2->Iniciar()) {
    REXLOG_ERROR("[gsr2] no se pudieron crear los recursos de Vulkan: se presenta como siempre");
    return nullptr;
  }
  REXLOG_INFO("[gsr2] recursos de Vulkan listos ({} ranuras de salida)", ranuras);
  return gsr2;
}

}  // namespace nfsmw::gsr2
