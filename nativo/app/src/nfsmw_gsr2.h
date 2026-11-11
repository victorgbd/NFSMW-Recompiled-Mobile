// nfsmw - Snapdragon Game Super Resolution 2 in the native renderer (cvar nfsmw_gsr2). See nfsmw_gsr2.cpp.

#pragma once

#include <cstdint>
#include <memory>

#include <rex/ui/vulkan/api.h>

namespace rex::ui::vulkan {
class VulkanDevice;
}

namespace nfsmw::gsr2 {

// nfsmw_gsr2, read once (the first time it is asked).
bool Activo();

// ---- Game threads ---------------------------------------------------------------------------------------
// Around sub_82445660 (the preparer fills the frame's command list; every eView::Update is inside).
void EmpezarLlenado();
void TerminarLlenado();
// From the sub_8243EB70 hook (eView::Update, with the projection written and before view x projection): adds
// the frame's jitter to the projection of the views drawn into the scene's target.
void AjustarProyeccion(uint8_t* base, uint32_t vista, uint32_t proyeccion);
// After eView::Update (sub_8243EC28 hook): view 1's view x projection for this frame.
void TrasUpdate(uint8_t* base, uint32_t vista, uint32_t bloque);

// ---- PM4 ring thread ------------------------------------------------------------------------------------
// The frame number VdSwap wrote in the PM4_XE_SWAP packet (0 if there was none).
void SwapEnAnillo(uint32_t secuencia);

struct Camara {
  bool valida = false;     // view 1 was updated in that frame (a 3D scene)
  double vp[16] = {};      // its view x projection without jitter (row vectors, row-major, Direct3D)
  float jitter_ndc[2] = {};  // what was added to the projection (NDC, y up); 0 if nothing was
  float tan_mitad_horizontal = 1.0f;
  bool invertida = false;  // depth 1 near, 0 far
};
// The camera of the frame whose Swap is being presented (after SwapEnAnillo).
const Camara& CamaraDelSwap();
// Whether the last presented frame went through GSR 2. The game only jitters the projection while it does:
// without the upscaler (GLES, a frame already at the screen's size) the jitter would only shake the image.
void AnotarUso(bool escalado);

// ---- The passes (Vulkan, ring thread) -------------------------------------------------------------------
struct EntradasVulkan {
  // The final frame (render size, with the HUD), GENERAL layout.
  VkImageView color = VK_NULL_HANDLE;
  uint32_t ancho = 0, alto = 0;
  // The scene's depth target (GENERAL), the part of it the frame comes from (in its texels).
  VkImage profundidad = VK_NULL_HANDLE;
  VkFormat formato_profundidad = VK_FORMAT_UNDEFINED;
  uint32_t profundidad_ancho = 0, profundidad_alto = 0;
  float rect_profundidad[4] = {};  // x0, y0, x1, y1
  // The composited scene the game copied right before the HUD (GENERAL), or VK_NULL_HANDLE.
  VkImageView escena = VK_NULL_HANDLE;
  bool escena_rb = false;  // red and blue swapped relative to color
};

class Gsr2Vulkan {
 public:
  // vs: the presenter's guest_output_triangle_strip_rect_vs; lineal: a linear clamp sampler. Both outlive this.
  // esperar: waits for every submitted output (before destroying images they may use).
  static std::unique_ptr<Gsr2Vulkan> Crear(const rex::ui::vulkan::VulkanDevice* dispositivo, VkShaderModule vs,
                                           VkSampler lineal, uint32_t ranuras, void (*esperar)(void*),
                                           void* esperar_dato);
  virtual ~Gsr2Vulkan() = default;
  // Records both passes into comandos for output slot 'ranura' (its previous use has finished). Returns the
  // view of the upscaled image (ancho_salida x alto_salida, R16G16B16A16_SFLOAT, GENERAL), or VK_NULL_HANDLE
  // if it could not (then the output goes as before).
  virtual VkImageView Grabar(VkCommandBuffer comandos, uint32_t ranura, const EntradasVulkan& entradas,
                             uint32_t ancho_salida, uint32_t alto_salida, const Camara& camara) = 0;
};

}  // namespace nfsmw::gsr2
