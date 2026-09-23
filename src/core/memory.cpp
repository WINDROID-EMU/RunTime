/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/platform.h>

#include <arm_neon.h>

#include <algorithm>
#include <cstring>

REXCVAR_DEFINE_BOOL(writable_executable_memory, true, "Memory",
                    "Allow executable memory to be writable")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace rex {
namespace memory {

bool IsWritableExecutableMemoryPreferred() {
  return REXCVAR_GET(writable_executable_memory);
}

void copy_128_aligned(void* dest, const void* src, size_t count) {
  std::memcpy(dest, src, count * 16);
}

// Although NEON offers vector rev instructions (like vrev32q_u8), they are
// slower in benchmarks. Also, using uint8x16xN_t wasn't any faster in the
// benchmarks, hence we use just one SIMD register to minimize residual
// processing.

void copy_and_swap_16_aligned(void* dst_ptr, const void* src_ptr, size_t count) {
  copy_and_swap_16_unaligned(dst_ptr, src_ptr, count);
}

void copy_and_swap_16_unaligned(void* dst_ptr, const void* src_ptr, size_t count) {
  auto dst = reinterpret_cast<uint8_t*>(dst_ptr);
  auto src = reinterpret_cast<const uint8_t*>(src_ptr);

  const uint8x16_t tbl_idx = vcombine_u8(vcreate_u8(UINT64_C(0x0607040502030001)),
                                         vcreate_u8(UINT64_C(0x0E0F0C0D0A0B0809)));

  while (count >= 8) {
    uint8x16_t data = vld1q_u8(src);
    data = vqtbl1q_u8(data, tbl_idx);
    vst1q_u8(dst, data);

    count -= 8;
    dst += 16;
    src += 16;
  }

  while (count > 0) {
    store_and_swap<uint16_t>(dst, load<uint16_t>(src));

    count--;
    dst += 2;
    src += 2;
  }
}

void copy_and_swap_32_aligned(void* dst, const void* src, size_t count) {
  copy_and_swap_32_unaligned(dst, src, count);
}

void copy_and_swap_32_unaligned(void* dst_ptr, const void* src_ptr, size_t count) {
  auto dst = reinterpret_cast<uint8_t*>(dst_ptr);
  auto src = reinterpret_cast<const uint8_t*>(src_ptr);

  const uint8x16_t tbl_idx = vcombine_u8(vcreate_u8(UINT64_C(0x405060700010203)),
                                         vcreate_u8(UINT64_C(0x0C0D0E0F08090A0B)));

  while (count >= 4) {
    uint8x16_t data = vld1q_u8(src);
    data = vqtbl1q_u8(data, tbl_idx);
    vst1q_u8(dst, data);

    count -= 4;
    dst += 16;
    src += 16;
  }

  while (count > 0) {
    store_and_swap<uint32_t>(dst, load<uint32_t>(src));

    count--;
    dst += 4;
    src += 4;
  }
}

void copy_and_swap_64_aligned(void* dst, const void* src, size_t count) {
  copy_and_swap_64_unaligned(dst, src, count);
}

void copy_and_swap_64_unaligned(void* dst_ptr, const void* src_ptr, size_t count) {
  auto dst = reinterpret_cast<uint8_t*>(dst_ptr);
  auto src = reinterpret_cast<const uint8_t*>(src_ptr);

  const uint8x16_t tbl_idx = vcombine_u8(vcreate_u8(UINT64_C(0x0001020304050607)),
                                         vcreate_u8(UINT64_C(0x08090A0B0C0D0E0F)));

  while (count >= 2) {
    uint8x16_t data = vld1q_u8(src);
    data = vqtbl1q_u8(data, tbl_idx);
    vst1q_u8(dst, data);

    count -= 2;
    dst += 16;
    src += 16;
  }

  while (count > 0) {
    store_and_swap<uint64_t>(dst, load<uint64_t>(src));

    count--;
    dst += 8;
    src += 8;
  }
}

void copy_and_swap_16_in_32_aligned(void* dst, const void* src, size_t count) {
  return copy_and_swap_16_in_32_unaligned(dst, src, count);
}

void copy_and_swap_16_in_32_unaligned(void* dst_ptr, const void* src_ptr, size_t count) {
  auto dst = reinterpret_cast<uint16_t*>(dst_ptr);
  auto src = reinterpret_cast<const uint16_t*>(src_ptr);
  while (count > 0) {
    uint16_t word0 = *src++;
    uint16_t word1 = *src++;
    *dst++ = word1;
    *dst++ = word0;

    count--;
  }
}

}  // namespace memory
}  // namespace rex
