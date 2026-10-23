#!/usr/bin/env python3
"""
El memcpy nativo del SDK ([rexcrt] memcpy) con la semantica del memcpy del juego
cuando el origen y el destino se solapan.

    python tools/parche_memcpy.py            aplicar
    python tools/parche_memcpy.py --estado
    python tools/parche_memcpy.py --revertir

Toca un fichero del SDK:  src/kernel/crt/memory.cpp
No guarda .original: aplica y deshace por sustitucion de texto exacta.


POR QUE
=======

app/overrides.toml (y nativo/app/overrides.toml) cambian el memcpy del CRT del
juego, sub_826BE1B0, por el del SDK, que llama a std::memcpy. El del juego copia
hacia delante: bytes hasta alinear el destino, palabras de 4 bytes y los bytes
que queden. Y el juego lo usa con rangos que se solapan: sub_823A0728 quita un
controlador de animacion de la lista de activos (0x829187A8) copiando la cola un
hueco hacia atras.

std::memcpy no define ese caso. En el PC da igual (el de glibc y el de MSVC
copian bien con solape), pero el de bionic en arm64 duplica elementos: medido en
una AYN Thor (Snapdragon 8 Gen 2, Android 13), da otra lista en 1203 de 2016
formas de quitar un elemento de listas de 2 a 64. Asi un controlador parado
seguia en la lista, sin pista (+0x0C a 0), y al actualizarlo en el siguiente
fotograma sub_8237FE58 leia de la direccion 4: SIGSEGV en el hilo del juego,
siempre al poco de la primera cinematica de la carrera (la del inicio del modo
carrera, a los ~15 s de empezar).

El parche deja el mismo resultado que el del juego en todos los casos: sin
solape, std::memcpy; con el destino antes del origen, std::memmove (copiar hacia
delante lee cada byte antes de pisarlo); con el destino despues y solape, el
algoritmo del juego tal cual.
"""

import argparse
import os
import pathlib
import sys


FICHERO = "src/kernel/crt/memory.cpp"

BLOQUES = [
    (
        '''#include <cstring>

#include <rex/hook.h>
''',
        '''#include <cstdint>
#include <cstring>

#include <rex/hook.h>
''',
    ),
    (
        '''static void* native_memcpy(void* dst, const void* src, size_t n) {
  return std::memcpy(dst, src, n);
}
''',
        '''// PARCHE LOCAL - memcpy con solape (tools/parche_memcpy.py).
// El memcpy del CRT de la Xbox 360 copia hacia delante, y el juego cuenta con ello con rangos que se
// solapan: NFSMW quita un elemento de un array copiando la cola un hueco hacia atras (destino < origen).
// std::memcpy no define ese caso, y el de bionic en arm64 duplica elementos de verdad. El mismo resultado
// que el del juego en todos los casos:
//  - sin solape: std::memcpy;
//  - destino <= origen: copiar hacia delante lee cada byte antes de pisarlo, que es lo que da std::memmove;
//  - destino > origen con solape: el algoritmo del juego (bytes hasta alinear el destino, palabras de 4
//    bytes leidas antes de escribirlas y los bytes que queden), con volatile para que el compilador no lo
//    cambie por un memmove.
static void* native_memcpy(void* dst, const void* src, size_t n) {
  auto* d = static_cast<uint8_t*>(dst);
  const auto* s = static_cast<const uint8_t*>(src);
  if (d + n <= s || s + n <= d) {
    return std::memcpy(dst, src, n);
  }
  if (d <= s) {
    return std::memmove(dst, src, n);
  }
  volatile uint8_t* vd = d;
  const volatile uint8_t* vs = s;
  while (n && (reinterpret_cast<uintptr_t>(vd) & 3)) {
    *vd++ = *vs++;
    --n;
  }
  for (; n >= 4; n -= 4, vd += 4, vs += 4) {
    const uint8_t b0 = vs[0], b1 = vs[1], b2 = vs[2], b3 = vs[3];
    vd[0] = b0;
    vd[1] = b1;
    vd[2] = b2;
    vd[3] = b3;
  }
  while (n--) {
    *vd++ = *vs++;
  }
  return dst;
}
''',
    ),
]


def localizar_sdk():
    raiz = pathlib.Path(__file__).resolve().parent.parent
    # NFSMW_SDK apunta a otro arbol del SDK (el de Android, por ejemplo). Si
    # esta puesta se usa SOLO esa ruta: caer en silencio en el SDK de Windows
    # parchearia el arbol equivocado.
    otro = os.environ.get("NFSMW_SDK")
    candidatos = [pathlib.Path(otro)] if otro else [raiz.parent / "rexglue-sdk", raiz / "sdk"]
    for cand in candidatos:
        if (cand / FICHERO).exists():
            return cand
    sys.exit(f"[ERROR] No encuentro {FICHERO} del SDK.\n"
             "        Se busca en NFSMW_SDK, o en ..\\rexglue-sdk y .\\sdk")


def leer(f):
    with open(f, encoding="utf-8", newline="") as h:
        txt = h.read()
    eol = "\r\n" if "\r\n" in txt else "\n"
    return txt.replace("\r\n", "\n"), eol


def escribir(f, txt, eol):
    with open(f, "w", encoding="utf-8", newline="") as h:
        h.write(txt.replace("\n", eol))


def main():
    p = argparse.ArgumentParser(add_help=True)
    p.add_argument("--estado", action="store_true")
    p.add_argument("--revertir", action="store_true")
    args = p.parse_args()

    f = localizar_sdk() / FICHERO
    txt, eol = leer(f)
    puestos = sum(1 for _, nuevo in BLOQUES if nuevo in txt)

    if args.estado:
        estado = ("aplicado" if puestos == len(BLOQUES) else "sin aplicar" if puestos == 0
                  else f"A MEDIAS ({puestos}/{len(BLOQUES)})")
        print(f"  memcpy con solape          {estado}")
        return 0

    if args.revertir:
        if puestos == 0:
            print("[ok] memcpy con solape: no habia nada puesto")
            return 0
        for ancla, nuevo in BLOQUES:
            txt = txt.replace(nuevo, ancla)
        escribir(f, txt, eol)
        print(f"[ok] Quitados {puestos} bloques")
        return 0

    if puestos == len(BLOQUES):
        print("[ok] memcpy con solape: ya estaba")
        return 0
    if puestos:
        sys.exit(f"[ERROR] memcpy con solape esta a medias ({puestos}/{len(BLOQUES)}). "
                 "Revierte primero con --revertir.")
    for ancla, _ in BLOQUES:
        n = txt.count(ancla)
        if n != 1:
            sys.exit(f"[ERROR] Un anclaje aparece {n} veces en {FICHERO}, esperaba 1:\n"
                     f"        {ancla.splitlines()[0].strip()}\n"
                     "        El SDK habra cambiado. No he tocado nada.")
    for ancla, nuevo in BLOQUES:
        txt = txt.replace(ancla, nuevo, 1)
    escribir(f, txt, eol)
    print("[ok] Aplicado: el memcpy nativo copia como el del juego aunque haya solape")
    return 0


if __name__ == "__main__":
    sys.exit(main())
