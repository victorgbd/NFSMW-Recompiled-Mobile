// nfsmw - Lossless Scaling frame generation in the native renderer (cvar nfsmw_lsfg). See nfsmw_lsfg.cpp.

#pragma once

#include <cstdint>
#include <memory>

#include <rex/ui/vulkan/api.h>

namespace rex::ui {
class Presenter;
}
namespace rex::ui::vulkan {
class VulkanDevice;
}

namespace nfsmw::lsfg {

// nfsmw_lsfg, read once.
bool Activo();

// What the native renderer's output pass gives it (all owned by the renderer, alive while this lives).
struct RecursosSalida {
  VkRenderPass render_pass_salida = VK_NULL_HANDLE;  // into VulkanPresenter::kGuestOutputFormat
  VkPipeline pipeline_bilineal = VK_NULL_HANDLE;    // guest_output_bilinear_ps in that render pass
  VkPipelineLayout layout_pipeline = VK_NULL_HANDLE;
  VkDescriptorSetLayout layout_descriptores = VK_NULL_HANDLE;  // image 0, immutable sampler 1, buffer 2
  VkShaderModule vs = VK_NULL_HANDLE;  // guest_output_triangle_strip_rect_vs
};

class LsfgVulkan {
 public:
  // nullptr if the device lacks something or Lossless.dll cannot be read (logged).
  static std::unique_ptr<LsfgVulkan> Crear(const rex::ui::vulkan::VulkanDevice* dispositivo,
                                           const RecursosSalida& recursos);
  virtual ~LsfgVulkan() = default;

  // ---- PM4 ring thread, at each Swap ----
  // The image the output pass draws the frame into (kGuestOutputFormat, ancho x alto; created or resized here).
  // version: unique per image, for the renderer's framebuffer cache. false if frame generation cannot run now.
  virtual bool PrepararCuadro(uint32_t ancho, uint32_t alto, VkImageView& vista, uint64_t& version) = 0;
  // After the output pass into that image was submitted: generates the frames between the previous one and this
  // one, and hands them and this one to the pacing thread, which presents them spread over the frame interval.
  virtual void Generar(rex::ui::Presenter* presentador, uint32_t aspecto_x, uint32_t aspecto_y) = 0;
};

}  // namespace nfsmw::lsfg
