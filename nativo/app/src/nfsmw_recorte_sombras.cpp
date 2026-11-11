// nfsmw - shadow trimming: update the shadow maps 1 of every N frames
//
// ===========================================================================
//  Why
//  In the menu and in a race, the two 1600x1600 shadow maps are more than
//  half of each frame's resolve area (~5.6 of ~9.9 screens) and, in a race,
//  ~584 of ~1,820 draws. It is an optional trade of image quality for FPS.
//
//  The pass (recompiled code)
//    sub_82443B18  shadow pass. Called by sub_82445660 (race) and
//                  sub_82444D80 (menus). It leaves its parameters in globals
//                  and queues on the render queue (sub_823C8378) the commands
//                  for one or two shadow maps; in a race it draws both within
//                  the same call, it does not alternate between frames.
//  If a frame does not call it, the maps keep what was last drawn and the
//  scene uses them anyway: the shadow lags behind.
//
//  What this file does
//  With nfsmw_sombras_cada = N (1..8) only 1 of every N calls goes through.
//  With 1, the default, it behaves like the game. It applies immediately,
//  also from the settings menu (L+R+Right, NFSMW category), to compare live
//  with the F3 counter.
// ===========================================================================

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "nfsmw_gsr2.h"

REXCVAR_DEFINE_INT32(nfsmw_sombras_cada, 1, "NFSMW",
                     "Actualizar los mapas de sombras 1 de cada N fotogramas (1 = como el juego)")
    .range(1, 8);

/*
 * One cascade instead of two.
 *
 * sub_82443B18 takes a boolean in r4: with 0 it draws one shadow map, otherwise two. The game itself passes
 * 0 in the menus and 1 in a race. They are two complete 1600x1600 maps (views 13 and 14 of the table at
 * 0x82A38070), not two halves of one.
 *
 * Measured on the console: the whole pass costs 11.3 ms of real GPU time and ~8.7 ms of CPU with ~660 draws
 * per frame, and 91 % of that is per-draw cost, not pixels (which is why shrinking the map did not help).
 * Dropping one map removes its half of the draws, its pass and its 1600x1600 resolve.
 *
 * What is lost: the second map's texture stays frozen with whatever it last held, because the scene keeps
 * sampling both. If that map is the far-range one, distant shadows stay stuck while the car moves. This
 * must be checked in motion.
 */
REXCVAR_DEFINE_INT32(nfsmw_sombras_mapas, 2, "NFSMW",
                     "Mapas de sombras que dibuja el juego en carrera: 2 (como el juego) o 1. Con 1 se "
                     "ahorra la mitad de los dibujos del pase, pero el segundo mapa se queda congelado")
    .range(1, 2);

/*
 * The 30 FPS guard.
 *
 * The goal is not a fast average: it is that no frame drops below 30. With FIFO presentation that is
 * binary: if the work does not fit in 33.3 ms, the frame slips to the next vblank and shows for 50 ms.
 * There is no middle ground.
 *
 * What this cannot do: degrade the frame in flight. By the time its cost is known it has already been
 * recorded, submitted and is being presented. No mechanism can save the first spike. What it does prevent
 * is staying below 30 for a sustained period: as soon as the average of the last frames approaches the
 * ceiling, the next frame already carries less work.
 *
 * The lever is the shadow, and it is the only one: it is the only lever with the three properties needed.
 * It is decided at the start of the frame, on the guest thread, and it is a whole block.
 *
 * Why there is no flicker. An earlier version had a step that updated the shadows 1 of every 3 frames. On
 * the console the guard went to the top step and stayed there 99 % of the time (the work did not fit by a
 * long way), so skipping frames stopped being an occasional lifeline and became the normal state: the
 * shadows flickered, and it was obvious at first sight.
 *
 * So skipping shadow frames is not part of the ladder. The remaining steps change the shadow but do not
 * make it flicker:
 *   1. one map instead of two  -> the second one freezes, it does not flicker
 *   2. no cast shadows         -> less visible, but consistently so
 *
 * And stepping down requires 30 s with margin, not 2: oscillating every two seconds between "with
 * shadows" and "without shadows" looks worse than the drop it fixes.
 *
 * It is also the test. The report says which step it was on and how many times it had to act. If it says
 * "step 0 100 % of the time, 0 raises", the work fit easily and the guard was not needed. If it says
 * anything else, it tells exactly how much was missing.
 */
REXCVAR_DEFINE_BOOL(nfsmw_guardia_30, true, "NFSMW",
                    "Si un tramo se mantiene por debajo de 30 FPS, recortar las sombras en el fotograma "
                    "siguiente hasta volver a 30. Escalones: 1 un solo mapa, 2 sin sombras proyectadas. "
                    "Ninguno salta fotogramas, asi que no parpadea. Baja de escalon sola cuando sobra "
                    "margen (30 s) y nunca deshace lo que pongas tu");
REXCVAR_DEFINE_INT32(nfsmw_guardia_30_presupuesto_us, 31000, "NFSMW",
                     "Techo de fotograma en microsegundos para la guardia. 31000 deja 2,3 ms de margen sobre "
                     "los 33333 de un fotograma de 30 FPS")
    .range(16000, 66000);

/*
 * Distance culling of what goes into the shadow map.
 *
 * The cost of the shadow pass follows triangles, not draws. The first regression, over 22 intervals and
 * reproduced in two builds with loads 2.6x apart, gave 10,000 triangles = 0.80 ms real. The map draws
 * 90,000 triangles per frame, more than the visible image (67,000), at twice the triangles per object.
 * That is why shrinking the map did not help (-44 % area gave -9 % time): area is not the problem.
 *
 * ---------------------------------------------------------------------------------------------
 * The figure above is outdated. The right one is 0.556 ms real per 10,000 triangles.
 *
 * New regression over 17 ten-second race intervals in which the open area is constant (5.12 Mtexels =
 * the two 1600x1600 maps, 2.0 passes), so the only thing that varies is the triangle count:
 *
 *     ms_crudo = 0.03420 x k_triangulos + 0.023        r2 = 0.982   (51 to 96 k triangles)
 *     -> x1.627 -> 10,000 triangles = 0.556 ms real
 *
 * The drop from 0.80 is expected: the translator's branch merging (1806 -> 641 branches) came in between.
 * The cvar descriptions that promised savings based on 0.80 were inflated 1.4x; they are corrected below.
 *
 * The same fit answers the ZCULL question: the intercept is 0.023 ms raw = 0.037 ms real. So with both
 * 1600x1600 maps open and zero triangles the pass costs nothing measurable. The whole pass is geometry.
 * For contrast, the same fit against draws gives r2 = 0.404 and an intercept of 2.44 ms real, a model
 * with no physical meaning. The triangle count rules, and only it.
 *
 * ZCULL rejects fragments, not triangles: it does not touch setup, vertices or recording. Its ceiling in
 * this pass is those 0.037 ms. Even with the most generous bound (the map scale test, where -44 % area
 * gave -9 % time, so at most 20 % of the pass is per-fragment work: 0.74 ms real), ZCULL could only
 * remove the hidden fraction of that, and without near-to-far draw order from the light it would only
 * catch about half of it. Realistic ceiling: 0.1-0.25 ms. In exchange TRANSFER_DST would have to be
 * removed from the map, which is what disqualifies it in the driver, and that costs the 1600x1600 swap
 * (2.36 ms measured). Net loss, 10 to 1. The details are in nfsmw_nativo_destinos.cpp, next to the
 * TRANSFER_DST decision. Not to be reopened without new data.
 * ---------------------------------------------------------------------------------------------
 *
 * Where the lever is, read from the binary. `WorldModel::Render` decides whether to draw with
 *
 *     eView::GetPixelSize(pos, 35.0f) < view->PixelMinSize   ->  not drawn
 *
 * and `GetPixelSize` returns `radio * H / (distancia - radio)`. So the cutoff distance is
 *
 *     35 * (1 + H / PixelMinSize)
 *
 * `PixelMinSize` is field +36 of the view (view 13 at 0x82A38644 and view 14 at 0x82A386B4, view table at
 * 0x82A38070 with 112 bytes per entry) and the game recomputes it every frame in sub_8243EC28, so it has
 * to be written afterwards: right here, in the pass hook, which runs after that recomputation and before
 * drawing.
 *
 * Raising it shortens the distance at which an object stops casting a shadow. Shadows of small, distant
 * objects are lost, which are the least visible ones; nearby ones are untouched.
 */

/*
 * Where the extra triangles are: the shadow map does not reduce detail.
 *
 * Counted over 22 intervals, summing whole frames:
 *     shadow map       2,948 draws   1,193 k triangles  ->  405 triangles per draw
 *     visible scene   16,770 draws   1,321 k triangles  ->   79 triangles per draw
 * The shadow map puts 5.1 times more triangles per object than the visible image, and it takes 47 % of all
 * the frame's triangles with 15 % of the draws.
 *
 * And the split appears exactly when the race starts. In the menu and the first intervals the ratio is
 * 0.79x, 0.83x, 0.85x, 1.05x (the same detail in both); as soon as the car is driven it jumps to 2.9x,
 * 3.5x, 6.8x, 7.8x, 8.1x. That is exactly the signature of the scene lowering mesh detail with distance
 * and the shadow map not doing so: in the menu almost everything is close and both match.
 *
 * So the shadow pass is not expensive because it draws too much, but because it draws each object with
 * the full-detail mesh when its shadow covers four pixels. The lever is not removing objects: it is using
 * the same reduced mesh as the scene. That is not decided here (the native renderer only sees GPU
 * registers, with no object id or mesh level); it is decided in the guest.
 *
 * What does not work, so it is not repeated:
 *   - Filtering by the draw's triangle count: it would remove simple-mesh objects, which are exactly the
 *     cheapest ones. The opposite of what is needed.
 *   - Shrinking the map (nfsmw_nativo_sombras_escala): the pass area is worth 0.037 ms.
 *   - ZCULL: it rejects fragments, and there is no fragment cost here to reject.
 */
/*
 * The default is 150 (it was 100), for this reason.
 *
 * Measured in a race: the shadow map pushes 65.7 k triangles per frame, almost as many as the visible
 * scene (68.9 k), and it does so with 153 draws against the scene's 922. That is 405 triangles per object
 * against 79. The shadow map does not lower mesh detail with distance and the scene does: in the menu,
 * where everything is close, both match (0.79x-1.05x); as soon as the car is driven, the ratio jumps to
 * 2.9x, 6.8x, 8.1x.
 *
 * In other words, the pass draws every car and every building with the full-detail mesh to cast a shadow
 * that covers four pixels. At 0.556 ms real per 10,000 triangles, those 65.7 k are 3.65 ms of the 3.69
 * measured: the pass is geometry and nothing else.
 *
 * 150 shortens the distance at which an object stops casting a shadow to two thirds. Shadows of small,
 * distant objects are lost (the least visible ones); nearby ones are untouched. It stays at 150 rather
 * than 200 because a shadow that pops in as you approach is noticeable, and the game must keep looking
 * right. The log says how many triangles it removes ("k triangulos" in the C6 line of area per render
 * target type).
 */
REXCVAR_DEFINE_INT32(nfsmw_sombras_corte, 150, "NFSMW",
                     "Corte por distancia del mapa de sombras, en porcentaje. 100 = como el juego. 200 = el "
                     "doble de exigente, o sea la mitad de distancia: los objetos pequenos y lejanos dejan de "
                     "proyectar sombra. El pase cuesta 0,556 ms reales por cada 10.000 triangulos y son los "
                     "triangulos el 99 % de su coste, asi que aqui es donde se recorta de verdad");

namespace nfsmw::sombras_corte {
namespace {
constexpr uint32_t kVista13 = 0x82A38620;   // 0x82A38070 + 13 * 112
constexpr uint32_t kVista14 = 0x82A38690;
constexpr uint32_t kOffPixelMinSize = 36;
constexpr uint32_t kOffH = 12;              // projection scale, for the report
std::atomic<bool> g_anotado{false};

/*
 * The cutoff used to compound itself.
 *
 * An earlier version read PixelMinSize and wrote read * percentage / 100. That only works if the game
 * recomputes it between two of our calls. sub_8243EC28 does recompute it (it writes it as an integer at
 * vista+36: `fctiwz f9,f10` + `stfiwx f9,0,r9`), but nothing guarantees that it runs every frame or in
 * every mode. As soon as it skips one, with cutoff = 200 the value doubles again, and again, until it
 * saturates at 4096: from then on no object casts a shadow and the map comes out empty. Silently, because
 * the log was only written once (g_anotado).
 *
 * Fixed by keeping the value the game computes. If what the view holds is exactly what we wrote last
 * time, the game has not recomputed it: start from the saved original, not from our value. That way the
 * cutoff is always the same no matter how often the call repeats.
 */
uint32_t g_base_vista[2] = {0, 0};     // the PixelMinSize the game computes
uint32_t g_escrito_vista[2] = {0, 0};  // the last value we wrote

uint32_t Leer32(const uint8_t* base, uint32_t dir) {
  uint32_t v = 0;
  std::memcpy(&v, base + dir, sizeof(v));
  return __builtin_bswap32(v);
}
void Escribir32(uint8_t* base, uint32_t dir, uint32_t valor) {
  const uint32_t v = __builtin_bswap32(valor);
  std::memcpy(base + dir, &v, sizeof(v));
}
float ComoFloat(uint32_t v) {
  float f = 0.0f;
  std::memcpy(&f, &v, sizeof(f));
  return f;
}
}  // namespace

void Aplicar(uint8_t* base) {
  const int32_t porcentaje = REXCVAR_GET(nfsmw_sombras_corte);
  for (uint32_t i = 0; i < 2; ++i) {
    const uint32_t vista = i == 0 ? kVista13 : kVista14;
    const uint32_t leido = Leer32(base, vista + kOffPixelMinSize);
    if (leido == 0 || leido > 4096) {
      continue;  // does not look like a pixel size: leave it alone
    }
    // If the value is exactly what we wrote last time, the game has not recomputed it since then: the
    // original is the one we saved, not what we just read.
    const bool es_nuestro = g_escrito_vista[i] != 0 && leido == g_escrito_vista[i];
    const uint32_t original = (es_nuestro && g_base_vista[i] != 0) ? g_base_vista[i] : leido;
    g_base_vista[i] = original;
    if (porcentaje <= 100) {
      // Cutoff disabled: if we had modified it, restore the game's value and forget it.
      if (es_nuestro && original != leido) {
        Escribir32(base, vista + kOffPixelMinSize, original);
      }
      g_escrito_vista[i] = 0;
      continue;
    }
    const uint64_t crudo = uint64_t(original) * uint64_t(porcentaje) / 100u;
    const uint32_t nuevo = uint32_t(crudo > 4096 ? 4096 : crudo);
    Escribir32(base, vista + kOffPixelMinSize, nuevo);
    g_escrito_vista[i] = nuevo;
    if (!g_anotado.exchange(true)) {
      REXLOG_INFO("[recortes] sombras: corte por distancia {} -> {} px (H = {:.1f}; la distancia de corte "
                  "pasa de {:.0f} a {:.0f} unidades)",
                  original, nuevo, double(ComoFloat(Leer32(base, vista + kOffH))),
                  35.0 * (1.0 + double(ComoFloat(Leer32(base, vista + kOffH))) / double(original)),
                  35.0 * (1.0 + double(ComoFloat(Leer32(base, vista + kOffH))) / double(nuevo)));
    }
  }
}
}  // namespace nfsmw::sombras_corte

// The guard's line goes out inside the ring report; the report thread writes it
// (nfsmw_nativo_informes_diferidos, defined in nfsmw_nativo_sistema.cpp).
namespace nfsmw::nativo {
void InformeDiferido(std::string linea);
}  // namespace nfsmw::nativo

namespace nfsmw::guardia30 {
namespace {

constexpr unsigned kVentana = 32;        // frames examined to decide
constexpr unsigned kPermanencia = 60;    // minimum frames before stepping up (~2 s)
constexpr unsigned kPermanenciaBaja = 900;  // and 30 s before stepping down: oscillating looks worse
constexpr int kEscalonMax = 2;
constexpr double kHisteresisMs = 4.0;    // stepping down needs this much extra margin

double g_ventana[kVentana] = {};
unsigned g_pos = 0;
unsigned g_vistos = 0;
int g_escalon = 0;
unsigned g_en_escalon = 0;
std::atomic<int> g_escalon_publicado{0};
std::atomic<uint64_t> g_fotogramas_por_escalon[kEscalonMax + 1];
std::atomic<uint64_t> g_subidas{0};
std::atomic<uint64_t> g_bajadas{0};

}  // namespace

/*
 * A presented frame, with how long it took. Called by Presentar() on the ring thread, the only one that
 * enters here: the window needs no lock.
 */
void Latir(double ms) {
  if (!REXCVAR_GET(nfsmw_guardia_30)) {
    if (g_escalon != 0) {
      g_escalon = 0;
      g_escalon_publicado.store(0, std::memory_order_relaxed);
      REXLOG_INFO("[guardia30] apagada: se devuelven las sombras completas");
    }
    return;
  }
  g_ventana[g_pos] = ms;
  g_pos = (g_pos + 1) % kVentana;
  if (g_vistos < kVentana) {
    ++g_vistos;
    return;  // nothing is decided until the window is full
  }
  ++g_en_escalon;
  g_fotogramas_por_escalon[g_escalon].fetch_add(1, std::memory_order_relaxed);
  if (g_en_escalon < kPermanencia) {
    return;
  }
  double suma = 0.0;
  for (unsigned i = 0; i < kVentana; ++i) {
    suma += g_ventana[i];
  }
  const double media = suma / double(kVentana);
  const double techo = double(REXCVAR_GET(nfsmw_guardia_30_presupuesto_us)) / 1000.0;
  const bool puede_bajar = g_en_escalon >= kPermanenciaBaja;
  if (media > techo && g_escalon < kEscalonMax) {
    ++g_escalon;
    g_en_escalon = 0;
    g_escalon_publicado.store(g_escalon, std::memory_order_relaxed);
    g_subidas.fetch_add(1, std::memory_order_relaxed);
    REXLOG_INFO("[guardia30] {:.1f} ms de media por fotograma (techo {:.1f}): escalon {}", media, techo,
                g_escalon);
  } else if (puede_bajar && media < techo - kHisteresisMs && g_escalon > 0) {
    --g_escalon;
    g_en_escalon = 0;
    g_escalon_publicado.store(g_escalon, std::memory_order_relaxed);
    g_bajadas.fetch_add(1, std::memory_order_relaxed);
    REXLOG_INFO("[guardia30] {:.1f} ms de media por fotograma, sobra margen: escalon {}", media, g_escalon);
  }
}

int Escalon() { return g_escalon_publicado.load(std::memory_order_relaxed); }

/* The guard only tightens: if one map was already requested, it never goes back to two. */
int MapasEfectivos(int del_usuario) {
  return Escalon() >= 1 ? 1 : del_usuario;
}
/* Step 2: skip the shadow pass. Constant, no flicker. */
bool SinSombras(bool del_usuario) {
  return del_usuario || Escalon() >= 2;
}

void Informe() {
  uint64_t total = 0, v[kEscalonMax + 1];
  for (int i = 0; i <= kEscalonMax; ++i) {
    v[i] = g_fotogramas_por_escalon[i].load(std::memory_order_relaxed);
    total += v[i];
  }
  if (total == 0) {
    return;
  }
  ::nfsmw::nativo::InformeDiferido(fmt::format(
      "[guardia30] fotogramas por escalon: 0 (dos mapas) {} = {:.1f} % | 1 (un mapa) {} | "
      "2 (sin sombras) {} | subidas {} | bajadas {} -> {}",
      v[0], 100.0 * double(v[0]) / double(total), v[1], v[2],
      g_subidas.load(std::memory_order_relaxed), g_bajadas.load(std::memory_order_relaxed),
      v[0] == total ? "NO HIZO FALTA: el trabajo cabia en 33,3 ms"
                    : "*** hizo falta recortar: el trabajo NO cabia ***"));
}

}  // namespace nfsmw::guardia30

namespace nfsmw::recorte_sombras {
namespace {

std::atomic<uint32_t> g_llamadas{0};
std::atomic<int32_t> g_cada_anotado{1};
std::atomic<int32_t> g_mapas_anotado{2};
std::atomic<bool> g_dos_jugadores_anotado{false};

/*
 * Split screen. The two maps are one per player: views 13 and 14 are the shadow maps of views 1 and 2,
 * as 4 and 5 are their road reflections. sub_82445660 asks for both (r4 = 1) when views 1 and 2 hold a
 * scene, and in single player view 2 stays off (sub_8243FF30 leaves only 1 and 4). With two players,
 * dropping the second map freezes player 2's shadows (seen on the phone: the bottom half). So the single
 * map only applies while view 2 is off. Byte +8 is the view's "active" flag (nfsmw_recortes_carrera.cpp).
 */
constexpr uint32_t kVista2Activa = 0x82A38070 + 2 * 112 + 8;

}  // namespace
}  // namespace nfsmw::recorte_sombras

REX_EXTERN(__imp__sub_82443B18);
REX_HOOK_RAW(sub_82443B18) {
  using namespace nfsmw::recorte_sombras;
  // A single map, which is what the game itself does in the menus (r4 = 0). Not in split screen: there
  // the second map is player 2's.
  const bool dos_jugadores = base[kVista2Activa] != 0;
  if (dos_jugadores && !g_dos_jugadores_anotado.exchange(true, std::memory_order_relaxed)) {
    REXLOG_INFO("[recortes] sombras: pantalla dividida, los dos mapas (uno por jugador)");
  }
  if (!dos_jugadores && nfsmw::guardia30::MapasEfectivos(REXCVAR_GET(nfsmw_sombras_mapas)) <= 1) {
    if (g_mapas_anotado.exchange(1, std::memory_order_relaxed) != 1) {
      REXLOG_INFO("[recortes] sombras: un solo mapa en vez de dos");
    }
    ctx.r4.u64 = 0;
  } else {
    g_mapas_anotado.store(2, std::memory_order_relaxed);
  }
  nfsmw::sombras_corte::Aplicar(base);  // distance cutoff, before drawing
  const int32_t cada = REXCVAR_GET(nfsmw_sombras_cada);
  if (cada <= 1) {
    g_cada_anotado.store(1, std::memory_order_relaxed);
    __imp__sub_82443B18(ctx, base);
    return;
  }
  if (g_cada_anotado.exchange(cada, std::memory_order_relaxed) != cada) {
    REXLOG_INFO("[recortes] sombras: se actualizan 1 de cada {} fotogramas", cada);
  }
  if (g_llamadas.fetch_add(1, std::memory_order_relaxed) % uint32_t(cada) == 0) {
    __imp__sub_82443B18(ctx, base);
  }
}

/*
 * =================================================================================================
 *  The visible scene: the same per-object LOD, and why it is not simply raised here
 * =================================================================================================
 *
 * The mechanism is the same as the shadow cutoff above and the cubemap faces
 * (nfsmw_cubemap_detalle_minimo): `WorldModel::Render` and `CarRender` bail out before drawing when
 *
 *     eView::GetPixelSize(pos, 35.0f) < view->PixelMinSize
 *
 * with `GetPixelSize = radio * H / (distancia - radio)`. `PixelMinSize` is field +0x24 of the view and
 * `eView::eView()` sets it to 4 for every view. The scene view is view 1 (0x82A38070 + 1 * 112 =
 * 0x82A380E0), the same one nfsmw_recortes_carrera.cpp uses. It saves CPU and GPU at once because the
 * object never even reaches the draw list.
 *
 * Where it is written. The game recomputes it in sub_8243EC28 (`fctiwz` + `stfiwx` on vista+36), which
 * sub_82441100 calls once per active view per frame with the view in r4. So the hook goes on that function
 * and writes afterwards: the value read there is always the freshly recomputed one, never ours, and so,
 * unlike the shadow cutoff, there is no need to save the original here: an absolute value is written and
 * it cannot compound itself.
 *
 * Why it defaults to 0 (= like the game) and not to a higher value. The scene costs 22.55 ms real and it
 * is fragment shading: 6.5 M fragments over 0.92 M pixels. An object removed by the threshold is, by
 * definition, one that covers fewer than PixelMinSize pixels: going from 4 to 6 removes objects of at
 * most 36 pixels each. Even if 300 objects per frame went away (a quarter of the 1,207 the scene draws),
 * that would be 11 k fragments out of 6.5 M: 0.17 %, 0.04 ms. On the GPU this gains nothing, and the GPU
 * is what needs fixing in the scene pass.
 *
 * What it does gain is CPU: at 10.0 us per draw measured, 300 fewer draws are ~3 ms of CPU per frame. But
 * in a race the frame is GPU-bound (49.7 ms real GPU against ~29 of CPU), so those 3 ms do not turn into
 * a single FPS except in the alley stretches, which are CPU-bound.
 *
 * And what it costs visually. The distance at which an object of radius R disappears is
 * R * H / PixelMinSize. Raising the threshold does not suddenly erase distant objects: it brings that
 * distance closer in inverse proportion, so from 4 to 6 the cutoff distance drops 33 % for every object.
 * At 200 km/h (55 m/s) that means an object pops in about one second of travel closer than it does now,
 * and popping is the first thing anyone notices. On top of that the shadow map has its own cutoff
 * (nfsmw_sombras_corte): if the scene culls before the shadow does, the shadow of a missing object remains.
 *
 * So the lever exists, is in place and measured, but shipping it enabled would trade graphics for FPS
 * that do not materialize. The values:
 *
 *   value   cutoff distance       what is lost
 *   ------  --------------------  -------------------------------------------------------------
 *     4     like the game         nothing
 *     5     -20 %                 only what no longer reaches 4 output pixels: the scene draws at
 *                                 1280x720 and is resolved to 1024x576, so the game's 4 pixels
 *                                 are 3.2 in what is shown. The only value with a real argument.
 *     6     -33 %                 street furniture starts visibly popping in at mid distance
 *     8     -50 %                 clearly visible; only for measuring the savings ceiling
 *
 * The first time through, the view's H and the actual cutoff distance for each value are logged.
 */
REXCVAR_DEFINE_INT32(nfsmw_escena_detalle_minimo, 0, "NFSMW",
                     "Tamano minimo en pixeles para que un objeto se dibuje en la escena que se ve "
                     "(eView::PixelMinSize de la vista 1; el juego usa 4). 0 = dejar el valor del juego. "
                     "Subirlo ACERCA la distancia de corte de TODOS los objetos en proporcion inversa "
                     "(6 = -33 %), asi que se paga en popping; y en GPU casi no da nada, porque lo que "
                     "quita son objetos de menos de ese tamano en pixeles. Ver la tabla del fichero")
    .range(0, 64);

namespace nfsmw::escena_detalle {
namespace {

constexpr uint32_t kBaseVistas = 0x82A38070;
constexpr uint32_t kBytesPorVista = 112;
constexpr uint32_t kVistaEscena = 1;
constexpr uint32_t kDireccionEscena = kBaseVistas + kVistaEscena * kBytesPorVista;  // 0x82A380E0
constexpr uint32_t kOffH = 0x0C;          // float: the view's projection scale
constexpr uint32_t kOffCerca = 0x10;      // float
constexpr uint32_t kOffLejos = 0x14;      // float
constexpr uint32_t kOffDetalle = 0x24;    // int PixelMinSize
constexpr float kRadioDelJuego = 35.0f;   // the one WorldModel::Render passes to GetPixelSize

std::atomic<bool> g_anotada_vista{false};
std::atomic<int32_t> g_anotado_valor{0};

uint32_t Leer32(const uint8_t* base, uint32_t dir) {
  uint32_t v = 0;
  std::memcpy(&v, base + dir, sizeof(v));
  return __builtin_bswap32(v);
}
void Escribir32(uint8_t* base, uint32_t dir, uint32_t valor) {
  const uint32_t v = __builtin_bswap32(valor);
  std::memcpy(base + dir, &v, sizeof(v));
}
float ComoFloat(uint32_t v) {
  float f = 0.0f;
  std::memcpy(&f, &v, sizeof(f));
  return f;
}
double Corte(double h, double p) { return p > 0.0 ? kRadioDelJuego * (1.0 + h / p) : 0.0; }

}  // namespace

/* Called right after sub_8243EC28, with the view it just recomputed. */
void Aplicar(uint8_t* base, uint32_t vista) {
  if (vista != kDireccionEscena) {
    return;
  }
  const uint32_t leido = Leer32(base, vista + kOffDetalle);
  if (leido == 0 || leido > 4096) {
    return;  // does not look like a pixel size: touch nothing
  }
  if (!g_anotada_vista.exchange(true)) {
    const double h = double(ComoFloat(Leer32(base, vista + kOffH)));
    REXLOG_INFO("[recortes] escena (vista {}): PixelMinSize del juego {}, H {:.1f}, cerca {:.2f}, lejos "
                "{:.1f}; distancia de corte de un objeto de radio {:.0f}: con 4 {:.0f}, con 5 {:.0f}, con "
                "6 {:.0f}, con 8 {:.0f} unidades",
                kVistaEscena, leido, h, double(ComoFloat(Leer32(base, vista + kOffCerca))),
                double(ComoFloat(Leer32(base, vista + kOffLejos))), double(kRadioDelJuego),
                Corte(h, 4.0), Corte(h, 5.0), Corte(h, 6.0), Corte(h, 8.0));
  }
  const int32_t pedido = REXCVAR_GET(nfsmw_escena_detalle_minimo);
  // This setting only tightens: it never lowers the game's threshold, whatever the cvar says.
  if (pedido <= 0 || uint32_t(pedido) <= leido) {
    if (g_anotado_valor.exchange(0, std::memory_order_relaxed) != 0) {
      REXLOG_INFO("[recortes] escena: se devuelve el detalle minimo del juego ({})", leido);
    }
    return;
  }
  Escribir32(base, vista + kOffDetalle, uint32_t(pedido));
  if (g_anotado_valor.exchange(pedido, std::memory_order_relaxed) != pedido) {
    REXLOG_INFO("[recortes] escena: los objetos de menos de {} pixeles dejan de dibujarse (el juego usa "
                "{}): la distancia de corte se queda en el {:.0f} % de la suya",
                pedido, leido, 100.0 * double(leido) / double(pedido));
  }
}

}  // namespace nfsmw::escena_detalle

/*
 * eView::Update (or equivalent): recomputes the view passed in r4 and leaves PixelMinSize at +0x24.
 * sub_82441100 calls it once per active view per frame, before drawing anything. Hooking here is the
 * only way to guarantee our value is the last one written.
 */
namespace nfsmw::panoramico {
void EntrarUpdate(uint32_t bloque);  // nfsmw_panoramico.cpp: the view block for the Hor+ camera
void SalirUpdate();
}  // namespace nfsmw::panoramico

REX_EXTERN(__imp__sub_8243EC28);
REX_HOOK_RAW(sub_8243EC28) {
  const uint32_t vista = ctx.r4.u32;  // r4 may be clobbered inside: save it first
  const uint32_t bloque = ctx.r3.u32;
  nfsmw::panoramico::EntrarUpdate(bloque);
  __imp__sub_8243EC28(ctx, base);
  nfsmw::panoramico::SalirUpdate();
  nfsmw::gsr2::TrasUpdate(base, vista, bloque);  // nfsmw_gsr2: view 1's camera for this frame
  nfsmw::escena_detalle::Aplicar(base, vista);
}

/*
 * =================================================================================================
 *  The menu with shadows, so it can be tested on the PC (nfsmw_prueba_menu_sombras)
 * =================================================================================================
 *
 * sub_824455B8 draws the menu scene and chooses between two paths:
 *   sub_82444D80  the garage with the shadow pass (sub_82443B18 with r4 = 0: one 1600x1600 map)
 *   sub_82445300  the same garage without the shadow pass
 * It takes the first one if sub_822D71A8(*(0x82A2C900), 0x82077C2C) finds the object, its +28 is not null,
 * that object's +120 is 0 and the global pointer 0x82A2D1B4 is null (sub_8245DD60 fills it when opening a
 * video with sub_826D76A8, and sub_82287380 and sub_8245DE38 release it).
 *
 * On the console, with the profile loaded, it takes the first one (07CEA000 and 086AE000 resolved in 99 %
 * of menu frames). On the PC, without a profile, it takes the second: not a single shadow pass, which is
 * why the menu flicker could not be reproduced there. This setting evaluates the four conditions and, if
 * the only one failing is the video, hides it while the path is chosen. Testing only, on the PC; when off
 * it touches nothing.
 */
REXCVAR_DEFINE_BOOL(nfsmw_prueba_menu_sombras, false, "NFSMW",
                    "Pruebas (build 193): anota por que la escena del menu va con o sin pase de sombras y, si solo lo "
                    "impide el puntero de video 0x82A2D1B4, lo oculta al elegir para que el menu dibuje sus sombras "
                    "como en la consola con perfil. Solo para reproducir en el PC el parpadeo del menu")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace nfsmw::prueba_menu_sombras {
namespace {
constexpr uint32_t kPelicula = 0x82A2D1B4;
constexpr uint32_t kListaFrontal = 0x82A2C900;
constexpr uint32_t kNombreFrontal = 0x82077C2C;

std::atomic<uint64_t> g_con_sombras{0};
uint64_t g_llamadas = 0;
uint64_t g_forzadas = 0;
uint64_t g_con_sombras_anotadas = 0;
uint64_t g_llamadas_anotadas = 0;
uint32_t g_ultimo_estado = 0xFFFFFFFFu;

uint32_t Leer32(const uint8_t* base, uint32_t dir) {
  uint32_t v = 0;
  std::memcpy(&v, base + dir, sizeof(v));
  return __builtin_bswap32(v);
}
void Escribir32(uint8_t* base, uint32_t dir, uint32_t valor) {
  const uint32_t v = __builtin_bswap32(valor);
  std::memcpy(base + dir, &v, sizeof(v));
}
}  // namespace
}  // namespace nfsmw::prueba_menu_sombras

REX_EXTERN(__imp__sub_822D71A8);
REX_EXTERN(__imp__sub_82444D80);
REX_EXTERN(__imp__sub_824455B8);

// The menu scene with shadows: only counted.
REX_HOOK_RAW(sub_82444D80) {
  nfsmw::prueba_menu_sombras::g_con_sombras.fetch_add(1, std::memory_order_relaxed);
  __imp__sub_82444D80(ctx, base);
}

REX_HOOK_RAW(sub_824455B8) {
  using namespace nfsmw::prueba_menu_sombras;
  if (!REXCVAR_GET(nfsmw_prueba_menu_sombras)) {
    __imp__sub_824455B8(ctx, base);
    return;
  }
  // The four conditions, as the game checks them. sub_822D71A8 is a list search that moves the found item
  // to the front: calling it once more changes nothing. The registers are restored as they were.
  const PPCContext guardado = ctx;
  ctx.r3.u64 = Leer32(base, kListaFrontal);
  ctx.r4.u64 = kNombreFrontal;
  __imp__sub_822D71A8(ctx, base);
  const uint32_t objeto = ctx.r3.u32;
  ctx = guardado;
  const uint32_t hijo = objeto ? Leer32(base, objeto + 28) : 0;
  const uint32_t campo120 = hijo ? Leer32(base, hijo + 120) : 0xFFFFFFFFu;
  const uint32_t pelicula = Leer32(base, kPelicula);
  const bool solo_video = objeto && hijo && campo120 == 0 && pelicula != 0;
  ++g_llamadas;
  if (solo_video) {
    ++g_forzadas;
    Escribir32(base, kPelicula, 0);
  }
  __imp__sub_824455B8(ctx, base);
  if (solo_video && Leer32(base, kPelicula) == 0) {
    Escribir32(base, kPelicula, pelicula);  // restored if nobody changed it inside
  }
  const uint32_t estado = (objeto ? 1u : 0u) | (hijo ? 2u : 0u) | (campo120 == 0 ? 4u : 0u) | (pelicula ? 8u : 0u);
  const uint64_t con_sombras = g_con_sombras.load(std::memory_order_relaxed);
  if (estado != g_ultimo_estado || g_llamadas - g_llamadas_anotadas >= 300) {
    REXLOG_INFO("[prueba] menu con sombras (build 193): objeto {:08X}, +28 {:08X}, +120 {:08X}, video {:08X} -> {}; "
                "{} llamadas desde la linea anterior, {} con sombras, {} forzadas desde el arranque",
                objeto, hijo, campo120, pelicula,
                !objeto || !hijo || campo120 != 0 ? "SIN sombras (falla el objeto del frontal)"
                : pelicula                        ? "sin sombras por el video: se oculta el video al elegir"
                                                  : "con sombras, como en la consola",
                g_llamadas - g_llamadas_anotadas, con_sombras - g_con_sombras_anotadas, g_forzadas);
    g_ultimo_estado = estado;
    g_llamadas_anotadas = g_llamadas;
    g_con_sombras_anotadas = con_sombras;
  }
}
