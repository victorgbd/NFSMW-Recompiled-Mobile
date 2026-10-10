#!/usr/bin/env python3
"""
Diagnostico: donde guarda el juego la proporcion de pantalla (para un widescreen fix).

    python tools/diagnostico/parche_aspecto.py            aplicar
    python tools/diagnostico/parche_aspecto.py --estado
    python tools/diagnostico/parche_aspecto.py --revertir

Toca un fichero del arbol del motor nativo:  app/src/nfsmw_recorte_sombras.cpp
Es para medir, no para dejarlo puesto.


POR QUE
=======

Un widescreen fix como el de PC (WidescreenFixesPack) cambia la proporcion y el
campo de vision de la proyeccion 3D y recoloca el HUD. En la Xbox 360 el juego ya
es 16:9 a 1280x720, asi que no hay un 4:3 que corregir: hay que encontrar donde
vive el 16:9 (un float 1.7777778 = 0x3FE38E39 en .rdata/.data, o un 1280 / 720) y
que funcion lo lee. Eso solo se ve con el juego en marcha, y aqui no hay XEX.


QUE HACE
========

Cuelga del gancho de sub_8243EC28 (eView::Update, una vez por vista y fotograma),
que el motor nativo ya usa para el detalle minimo de la escena.

  nfsmw_diag_aspecto = true
      Una vez, barre la imagen del juego (0x82000000-0x82CD0000) buscando los
      floats 16/9 (0x3FE38E39), 4/3 (0x3FAAAAAB), 1280.0 y 720.0 y apunta en el log
      sus direcciones:   [aspecto] 16:9 (0x3FE38E39): 7 sitios: 0x82xxxxxx ...
      Y cada ~2 s apunta los campos de la vista de la escena (H, cerca, lejos, fov).

  nfsmw_diag_poke = "0x82xxxxxx=1.7777778;0x82yyyyyy=2.3"
      Cada fotograma escribe esos floats en esas direcciones del guest. Sirve para
      probar candidatos del barrido sin recompilar: se cambia el valor y se mira
      que hace la imagen (se estira? se ensancha el campo de vision?).

Se pasan igual que los demas cvars del motor nativo (--nfsmw_diag_aspecto=true).
Con los dos apagados no toca nada.
"""

import argparse
import pathlib
import sys

FICHERO = "app/src/nfsmw_recorte_sombras.cpp"

DEFINICION_ANCLA = '''REX_EXTERN(__imp__sub_8243EC28);
'''

DEFINICION_NUEVA = '''// DIAGNOSTICO - aspecto: ver tools/diagnostico/parche_aspecto.py.
REXCVAR_DEFINE_BOOL(nfsmw_diag_aspecto, false, "NFSMW",
                    "Diagnostico: barre la memoria del juego buscando la proporcion 16:9 y apunta los campos de la vista");
REXCVAR_DEFINE_STRING(nfsmw_diag_poke, "", "NFSMW",
                      "Diagnostico: floats a escribir en el guest cada fotograma, 'dir=valor;dir=valor'");

namespace nfsmw::diag_aspecto {
namespace {
constexpr uint32_t kInicioImagen = 0x82000000;
constexpr uint32_t kFinImagen = 0x82CD0000;
constexpr uint32_t kMaxSitios = 64;

uint32_t LeerGuest(const uint8_t* base, uint32_t dir) {
  uint32_t v = 0;
  std::memcpy(&v, base + dir, sizeof(v));
  return __builtin_bswap32(v);
}
float FloatDe(uint32_t v) {
  float f = 0.0f;
  std::memcpy(&f, &v, sizeof(f));
  return f;
}

void Barrer(const uint8_t* base) {
  const struct {
    float valor;
    const char* nombre;
  } floats[] = {{1.7777778f, "16:9 (1.7777778)"}, {1.3333334f, "4:3 (1.3333334)"},
                {1280.0f, "ancho 1280.0"},        {720.0f, "alto 720.0"},
                {0.5625f, "9/16 (0.5625)"}};
  for (const auto& f : floats) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f.valor, sizeof(bits));
    uint32_t total = 0;
    std::string sitios;
    for (uint32_t dir = kInicioImagen; dir < kFinImagen; dir += 4) {
      if (LeerGuest(base, dir) != bits) {
        continue;
      }
      if (total++ < kMaxSitios) {
        sitios += fmt::format(" 0x{:08X}", dir);
      }
    }
    REXLOG_INFO("[aspecto] {} (0x{:08X}): {} sitios{}{}", f.nombre, bits, total, total ? ":" : "", sitios);
  }
}

void Vista(const uint8_t* base, uint32_t vista) {
  // eView: H +0x0C, cerca +0x10, lejos +0x14, fovbias +0x18, fov +0x1C (grados).
  REXLOG_INFO("[aspecto] vista de la escena: H {:.2f} cerca {:.3f} lejos {:.1f} fovbias {:.3f} fov {:.2f}",
              double(FloatDe(LeerGuest(base, vista + 0x0C))), double(FloatDe(LeerGuest(base, vista + 0x10))),
              double(FloatDe(LeerGuest(base, vista + 0x14))), double(FloatDe(LeerGuest(base, vista + 0x18))),
              double(FloatDe(LeerGuest(base, vista + 0x1C))));
}

void Escribir(uint8_t* base, const std::string& texto) {
  size_t i = 0;
  while (i < texto.size()) {
    size_t fin = texto.find(';', i);
    if (fin == std::string::npos) {
      fin = texto.size();
    }
    const std::string par = texto.substr(i, fin - i);
    i = fin + 1;
    const size_t igual = par.find('=');
    if (igual == std::string::npos) {
      continue;
    }
    const uint32_t dir = uint32_t(std::strtoul(par.c_str(), nullptr, 0));
    const float valor = std::strtof(par.c_str() + igual + 1, nullptr);
    if (dir < kInicioImagen || dir + 4 > 0x83000000 || (dir & 3) != 0) {
      continue;  // solo direcciones del guest, alineadas
    }
    uint32_t bits = 0;
    std::memcpy(&bits, &valor, sizeof(bits));
    bits = __builtin_bswap32(bits);
    // Las constantes suelen estar en .rdata, que el runtime deja de solo lectura: sin esto la
    // escritura es "Unhandled guest access violation" (el 16:9 de la PAL, 0x8200FAC0, esta ahi).
    // Se abre la pagina para escribir y se queda asi: es para medir.
    static const uintptr_t pagina = uintptr_t(sysconf(_SC_PAGESIZE));
    void* inicio = reinterpret_cast<void*>(uintptr_t(base + dir) & ~(pagina - 1));
    mprotect(inicio, pagina, PROT_READ | PROT_WRITE);
    std::memcpy(base + dir, &bits, sizeof(bits));
  }
}
}  // namespace

void Aplicar(uint8_t* base, uint32_t vista) {
  static std::atomic<bool> barrido{false};
  static std::atomic<uint32_t> llamadas{0};
  const std::string poke = REXCVAR_GET(nfsmw_diag_poke);
  if (!poke.empty()) {
    Escribir(base, poke);
  }
  if (!REXCVAR_GET(nfsmw_diag_aspecto)) {
    return;
  }
  if (!barrido.exchange(true)) {
    Barrer(base);
  }
  // Una vez por vista y fotograma, y solo la de la escena (vista 1, a 112 bytes de la 0).
  if (vista == 0x82A380E0 && (llamadas.fetch_add(1, std::memory_order_relaxed) % 120) == 0) {
    Vista(base, vista);
  }
}
}  // namespace nfsmw::diag_aspecto

REX_EXTERN(__imp__sub_8243EC28);
'''

LLAMADA_ANCLA = '''  nfsmw::escena_detalle::Aplicar(base, vista);
}
'''

LLAMADA_NUEVA = '''  nfsmw::escena_detalle::Aplicar(base, vista);
  nfsmw::diag_aspecto::Aplicar(base, vista);  // DIAGNOSTICO - aspecto
}
'''

INCLUDE_ANCLA = '''#include <rex/cvar.h>
#include <rex/hook.h>
'''

INCLUDE_NUEVO = '''#include <cstdlib>

#include <sys/mman.h>
#include <unistd.h>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/hook.h>
'''

BLOQUES = [
    ("fmt", INCLUDE_ANCLA, INCLUDE_NUEVO),
    ("cvars y barrido", DEFINICION_ANCLA, DEFINICION_NUEVA),
    ("llamada desde eView::Update", LLAMADA_ANCLA, LLAMADA_NUEVA),
]


def leer(f):
    with open(f, encoding="utf-8", newline="") as h:
        txt = h.read()
    eol = "\r\n" if "\r\n" in txt else "\n"
    return txt.replace("\r\n", "\n"), eol


def main():
    raiz = pathlib.Path(__file__).resolve().parents[2]
    p = argparse.ArgumentParser()
    p.add_argument("--arbol", default=str(raiz.parent / "nfsmw-android"),
                   help="el arbol del motor nativo (con app/)")
    p.add_argument("--estado", action="store_true")
    p.add_argument("--revertir", action="store_true")
    args = p.parse_args()

    f = pathlib.Path(args.arbol) / FICHERO
    if not f.exists():
        sys.exit(f"[ERROR] No encuentro {f}")
    txt, eol = leer(f)

    if args.estado:
        for nombre, _, nuevo in BLOQUES:
            print(f"  {'si' if nuevo in txt else 'NO':>2}  {nombre}")
        return 0

    if args.revertir:
        for _, ancla, nuevo in reversed(BLOQUES):
            txt = txt.replace(nuevo, ancla)
    else:
        faltan = [b for b in BLOQUES if b[2] not in txt]
        for nombre, ancla, _ in faltan:
            n = txt.count(ancla)
            if n != 1:
                sys.exit(f"[ERROR] El anclaje de '{nombre}' aparece {n} veces en {FICHERO}, esperaba 1. "
                         f"No he tocado nada.")
        for nombre, ancla, nuevo in faltan:
            txt = txt.replace(ancla, nuevo)
            print(f"[ok] Aplicado: {nombre}")
    with open(f, "w", encoding="utf-8", newline="") as h:
        h.write(txt.replace("\n", eol))
    return 0


if __name__ == "__main__":
    sys.exit(main())
