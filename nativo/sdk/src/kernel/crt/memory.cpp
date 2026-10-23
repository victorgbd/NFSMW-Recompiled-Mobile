/**
 * @file        kernel/crt/memory.cpp
 *
 * @brief       Native memory operation hooks -- replaces recompiled PPC
 *              implementations of memcpy, memmove, memset, etc.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 * @license     BSD 3-Clause License
 */
#include <cstdint>
#include <cstring>

#include <rex/hook.h>

namespace rex::kernel::crt {

// ---------------------------------------------------------------------------
// Standard memory operations
// ---------------------------------------------------------------------------

// PARCHE LOCAL - memcpy con solape (tools/parche_memcpy.py).
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

static void* native_memmove(void* dst, const void* src, size_t n) {
  return std::memmove(dst, src, n);
}

static void* native_memset(void* dst, int val, size_t n) {
  return std::memset(dst, val, n);
}

static void* native_memchr(const void* ptr, int val, size_t n) {
  return const_cast<void*>(std::memchr(ptr, val, n));
}

// ---------------------------------------------------------------------------
// Xbox/VMX-optimized variants (same semantics, native speed)
// ---------------------------------------------------------------------------

static void* native_XMemCpy(void* dst, const void* src, size_t n) {
  return std::memcpy(dst, src, n);
}

static void* native_XMemSet(void* dst, int val, size_t n) {
  return std::memset(dst, val, n);
}

static void* native_XMemSet128(void* dst, int val, size_t n) {
  return std::memset(dst, val, n);
}

static void* native_memset_vmx(void* dst, int val, size_t n) {
  return std::memset(dst, val, n);
}

// ---------------------------------------------------------------------------
// Secure variants (return errno_t)
// ---------------------------------------------------------------------------

static int native_memcpy_s(void* dst, size_t dstsz, const void* src, size_t count) {
  if (!dst || !src || count > dstsz)
    return 22;  // EINVAL
  std::memcpy(dst, src, count);
  return 0;
}

static int native_memmove_s(void* dst, size_t dstsz, const void* src, size_t count) {
  if (!dst || !src || count > dstsz)
    return 22;  // EINVAL
  std::memmove(dst, src, count);
  return 0;
}

}  // namespace rex::kernel::crt

REX_HOOK(rexcrt_memcpy, rex::kernel::crt::native_memcpy)
REX_HOOK(rexcrt_memmove, rex::kernel::crt::native_memmove)
REX_HOOK(rexcrt_memset, rex::kernel::crt::native_memset)
REX_HOOK(rexcrt_memchr, rex::kernel::crt::native_memchr)
REX_HOOK(rexcrt_XMemCpy, rex::kernel::crt::native_XMemCpy)
REX_HOOK(rexcrt_XMemSet, rex::kernel::crt::native_XMemSet)
REX_HOOK(rexcrt_XMemSet128, rex::kernel::crt::native_XMemSet128)
REX_HOOK(rexcrt_memset_vmx, rex::kernel::crt::native_memset_vmx)
REX_HOOK(rexcrt_memcpy_s, rex::kernel::crt::native_memcpy_s)
REX_HOOK(rexcrt_memmove_s, rex::kernel::crt::native_memmove_s)
