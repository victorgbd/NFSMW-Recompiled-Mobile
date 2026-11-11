// nfsmw - the camera wider than 16:9 (Hor+), with nfsmw_proporcion_pantalla
//
// ===========================================================================
//  What the game does (sub_8243EC28, eView::Update, read in the recompiled code and the XEX image)
//
//  r28 = eView, r31 = the view block: projection at r31+64, view x projection at r31+128 and, after
//  it, the frustum planes. The proportion is not a constant: it is the height / width of the view's
//  render target (table 0x82C3F590, 384 bytes per view id [eView+4], object at +368 with the width at
//  +16 and the height at +20; 640x480 if there is none). The angles are in u16 units (65536 = a turn,
//  sine table sub_821568F8) and fov = u16 at camera+196 ([eView+64] + 196):
//      horizontal  fov * 1.015 * 0.5             -> [0][0] = 1 / tan
//      vertical    fov * (height / width) * 1.1165 * 0.5   -> [1][1] = -1 / tan
//  (1.015 and 1.1165 for the player views 1, 2, 4, 5 and 17; 1.5022 and another horizontal with the
//  byte 0x828F4A8F; 1 and 1 for the others). The horizontal angle only depends on the fov: it is
//  Vert-, like the PC version. Drawing wider as is shows the same sides and crops top and bottom.
//
//  Between storing the projection and multiplying it by the view it calls sub_8243EB70 (only from
//  there, lr 0x8243EFA8), which returns H = (width / 2) / tan(fov / 2) for eView+12 (the LOD and the
//  minimum pixel size). That is where it is fixed: everything computed afterwards (view x projection,
//  frustum planes, culling) already sees the wide camera.
//
//  What is changed, only for views whose target is the panoramic one (width = AnchoPanoramico()), with
//  k = width / the 16:9 width of the same height (1600x720: 1.25):
//      [0][0] / k                       the horizontal widens by k (Hor+)
//      [1][1] as with a 16:9 target     the vertical angle grows back by k (it is linear in the
//                                       proportion: v = atan(1 / |[1][1]|), v * k)
//      H / k                            the same LOD and culling distances as at 16:9
//  The rest of the frame (HUD, menus, post-processing) is not touched here: prototype.
// ===========================================================================

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <rex/hook.h>
#include <rex/logging.h>

#include "nfsmw_gsr2.h"

namespace nfsmw::render_targets {
bool Panoramico();
uint32_t AnchoPanoramico();
double FactorPanoramico();
}  // namespace nfsmw::render_targets

namespace nfsmw::panoramico {
namespace {

constexpr uint32_t kRetornoEnUpdate = 0x8243EFA8;  // the only call, in sub_8243EC28
constexpr uint32_t kTablaPases = 0x82C3F590;
constexpr uint32_t kBytesPorPase = 384;
constexpr uint32_t kOffDestino = 368;
constexpr uint32_t kMaxVistas = 64;  // only for the logs

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

// One line per view id the first time it is seen (corrected or not), to know which ones the game uses.
std::atomic<uint64_t> g_vistas_anotadas{0};

// The view block (r3 of eView::Update) while it runs on this thread. In the generated code r28-r31 are local
// variables, not context registers: the hook below cannot read r30/r31, so it gets the block from here.
thread_local uint32_t t_bloque = 0;
bool PrimeraVez(uint32_t id) {
  if (id >= kMaxVistas) {
    return false;
  }
  const uint64_t bit = uint64_t(1) << id;
  return (g_vistas_anotadas.fetch_or(bit, std::memory_order_relaxed) & bit) == 0;
}

}  // namespace

// From the eView::Update hook (nfsmw_recorte_sombras.cpp), around the original.
void EntrarUpdate(uint32_t bloque) { t_bloque = bloque; }
void SalirUpdate() { t_bloque = 0; }
uint32_t BloqueActual() { return t_bloque; }

// The view (eView, r3) of the shadow map fits while they run (sub_824553A0 and sub_82456198, below).
thread_local uint32_t t_vista_ajuste = 0;
void EntrarAjuste(uint32_t vista) { t_vista_ajuste = vista; }
void SalirAjuste() { t_vista_ajuste = 0; }
uint32_t VistaAjuste() { return t_vista_ajuste; }

// Hor+ on a projection of the game (1 / tan in [0][0], -1 / tan in [1][1]) if the view's target has the
// panoramic proportion. Returns k (1 = untouched). 'quien' only for the log.
double Corregir(uint8_t* base, uint32_t vista, uint32_t proyeccion, const char* quien) {
  const uint32_t id = Leer32(base, vista + 4);
  const uint32_t destino = Leer32(base, kTablaPases + kBytesPorPase * id + kOffDestino);
  const uint32_t ancho = destino ? Leer32(base, destino + 16) : 640;
  const uint32_t alto = destino ? Leer32(base, destino + 20) : 480;
  // The panoramic proportion, at any size: the scene (2400x1080) and its half-size copies (1200x540, view 4).
  const double proporcion = nfsmw::render_targets::FactorPanoramico() * 16.0 / 9.0;
  if (alto == 0 || std::fabs(double(ancho) / double(alto) - proporcion) > proporcion * 0.02) {
    if (PrimeraVez(id)) {
      REXLOG_INFO("[panoramico] vista {}: destino {}x{}, se deja como esta", id, ancho, alto);
    }
    return 1.0;
  }
  const double k = nfsmw::render_targets::FactorPanoramico();
  const float sx = LeerFloat(base, proyeccion + 0);
  const float sy = LeerFloat(base, proyeccion + 20);
  if (!(sx > 0.0f) || sy == 0.0f || !std::isfinite(sx) || !std::isfinite(sy)) {
    return 1.0;
  }
  const double vertical = std::atan(1.0 / std::fabs(double(sy)));
  const double vertical_169 = std::min(vertical * k, 1.55);  // below 89 degrees, whatever comes in
  const float nuevo_sx = float(double(sx) / k);
  const float nuevo_sy = float(std::copysign(1.0 / std::tan(vertical_169), double(sy)));
  EscribirFloat(base, proyeccion + 0, nuevo_sx);
  EscribirFloat(base, proyeccion + 20, nuevo_sy);
  static std::atomic<uint32_t> avisos{0};
  if (avisos.fetch_add(1, std::memory_order_relaxed) < 12) {
    REXLOG_INFO("[panoramico] {} vista {}: destino {}x{}, k {:.4f}: [0][0] {:.4f} -> {:.4f}, [1][1] {:.4f} -> "
                "{:.4f} (vertical {:.2f} -> {:.2f} grados)",
                quien, id, ancho, alto, k, double(sx), double(nuevo_sx), double(sy), double(nuevo_sy),
                vertical * 2.0 * 57.29577951308232, vertical_169 * 2.0 * 57.29577951308232);
  }
  return k;
}

}  // namespace nfsmw::panoramico

// H of the view (sub_8243EB70), called by eView::Update with the projection already written and before
// multiplying it by the view. r3 = eView; the projection is at the view block + 64.
REX_EXTERN(__imp__sub_8243EB70);
REX_HOOK_RAW(sub_8243EB70) {
  const uint32_t vista = ctx.r3.u32;
  const uint32_t bloque = nfsmw::panoramico::BloqueActual();
  const uint32_t proyeccion = bloque + 64;
  const bool desde_update = uint32_t(ctx.lr) == nfsmw::panoramico::kRetornoEnUpdate && bloque != 0;
  __imp__sub_8243EB70(ctx, base);
  if (desde_update && nfsmw::render_targets::Panoramico()) {
    const double k = nfsmw::panoramico::Corregir(base, vista, proyeccion, "camara");
    ctx.f1.f64 /= k;  // H as at 16:9: the same LOD and culling distances
  }
  if (desde_update) {
    nfsmw::gsr2::AjustarProyeccion(base, vista, proyeccion);  // nfsmw_gsr2: the frame's jitter
  }
}

/*
 * The shadow map fits: sub_824553A0 and sub_82456198 build their own copy of the camera's projection (same
 * formula: fov at camera+196, height / width of the view's target) to fit the shadow maps to what the camera
 * sees, and both pass it to sub_82456D60 (frustum planes from a projection, r4; called only from these two).
 * Without the Hor+ correction the maps cover the old frustum, not the visible one: the shadows did not match
 * the image (seen doubled). The projection is corrected in place in the caller's frame before the planes, so
 * everything those functions compute afterwards uses it too. r3 = eView in both.
 */
REX_EXTERN(__imp__sub_824553A0);
REX_HOOK_RAW(sub_824553A0) {
  nfsmw::panoramico::EntrarAjuste(ctx.r3.u32);
  __imp__sub_824553A0(ctx, base);
  nfsmw::panoramico::SalirAjuste();
}
REX_EXTERN(__imp__sub_82456198);
REX_HOOK_RAW(sub_82456198) {
  nfsmw::panoramico::EntrarAjuste(ctx.r3.u32);
  __imp__sub_82456198(ctx, base);
  nfsmw::panoramico::SalirAjuste();
}
REX_EXTERN(__imp__sub_82456D60);
REX_HOOK_RAW(sub_82456D60) {
  const uint32_t vista = nfsmw::panoramico::VistaAjuste();
  const uint32_t retorno = uint32_t(ctx.lr);
  if (vista != 0 && (retorno == 0x82455540 || retorno == 0x824563E8) && nfsmw::render_targets::Panoramico()) {
    nfsmw::panoramico::Corregir(base, vista, ctx.r4.u32, retorno == 0x82455540 ? "sombras 1" : "sombras 2");
  }
  __imp__sub_82456D60(ctx, base);
}
