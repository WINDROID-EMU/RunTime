/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

#include <rex/dbg.h>
#include <rex/graphics/pipeline/texture/conversion.h>
#include <rex/hash.h>
#include <rex/logging.h>
#include <rex/math.h>
#include <rex/memory.h>

namespace rex::graphics::texture_conversion {

using namespace rex::graphics::xenos;

void CopySwapBlock(xenos::Endian endian, void* output, const void* input, size_t length) {
  switch (endian) {
    case xenos::Endian::k8in16:
      memory::copy_and_swap_16_unaligned(output, input, length / 2);
      break;
    case xenos::Endian::k8in32:
      memory::copy_and_swap_32_unaligned(output, input, length / 4);
      break;
    case xenos::Endian::k16in32:  // Swap high and low 16 bits within a 32 bit
                                  // word
      memory::copy_and_swap_16_in_32_unaligned(output, input, length);
      break;
    default:
    case xenos::Endian::kNone:
      std::memcpy(output, input, length);
      break;
  }
}

void ConvertTexelCTX1ToR8G8(xenos::Endian endian, void* output, const void* input, size_t length) {
  // https://fileadmin.cs.lth.se/cs/Personal/Michael_Doggett/talks/unc-xenos-doggett.pdf
  // (R is in the higher bits, according to how this format is used in
  //  4D5307E6).
  union {
    uint8_t data[8];
    struct {
      uint8_t g0, r0, g1, r1;
      uint32_t xx;
    };
  } block;
  static_assert(sizeof(block) == 8, "CTX1 block mismatch");

  const uint32_t bytes_per_block = 8;
  CopySwapBlock(endian, block.data, input, bytes_per_block);

  // Fast integer approximation of (2/3*a + 1/3*b) and (1/3*a + 2/3*b)
  uint8_t cr[4] = {block.r0, block.r1,
                   static_cast<uint8_t>(((2 * block.r0 + block.r1) * 85 + 128) >> 8),
                   static_cast<uint8_t>(((block.r0 + 2 * block.r1) * 85 + 128) >> 8)};
  uint8_t cg[4] = {block.g0, block.g1,
                   static_cast<uint8_t>(((2 * block.g0 + block.g1) * 85 + 128) >> 8),
                   static_cast<uint8_t>(((block.g0 + 2 * block.g1) * 85 + 128) >> 8)};

  auto output_bytes = static_cast<uint8_t*>(output);
  for (uint32_t oy = 0; oy < 4; ++oy) {
    uint32_t row_shift = oy * 8;
    for (uint32_t ox = 0; ox < 4; ++ox) {
      uint8_t xx = (block.xx >> (row_shift + (ox * 2))) & 3;
      output_bytes[(oy * length) + (ox * 2) + 0] = cr[xx];
      output_bytes[(oy * length) + (ox * 2) + 1] = cg[xx];
    }
  }
}

void ConvertTexelDXT3AToDXT3(xenos::Endian endian, void* output, const void* input, size_t length) {
  const uint32_t bytes_per_block = 16;
  auto output_bytes = static_cast<uint8_t*>(output);
  CopySwapBlock(endian, &output_bytes[0], input, 8);
  std::memset(&output_bytes[8], 0, 8);
}

// https://github.com/BinomialLLC/crunch/blob/ea9b8d8c00c8329791256adafa8cf11e4e7942a2/inc/crn_decomp.h#L4108
static uint32_t TiledOffset2DRow(uint32_t y, uint32_t width, uint32_t log2_bpp) {
  uint32_t macro = ((y / 32) * (width / 32)) << (log2_bpp + 7);
  uint32_t micro = ((y & 6) << 2) << log2_bpp;
  return macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 8) << (3 + log2_bpp)) +
         ((y & 1) << 4);
}

static uint32_t TiledOffset2DColumn(uint32_t x, uint32_t y, uint32_t log2_bpp,
                                    uint32_t base_offset) {
  uint32_t macro = (x / 32) << (log2_bpp + 7);
  uint32_t micro = (x & 7) << log2_bpp;
  uint32_t offset = base_offset + (macro + ((micro & ~0xF) << 1) + (micro & 0xF));
  return ((offset & ~0x1FF) << 3) + ((offset & 0x1C0) << 2) + (offset & 0x3F) + ((y & 16) << 7) +
         (((((y & 8) >> 2) + (x >> 3)) & 3) << 6);
}

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

class UntileWorkerPool {
 public:
  static UntileWorkerPool& Instance() {
    static UntileWorkerPool pool;
    return pool;
  }

  void ParallelFor(uint32_t total_count, const std::function<void(uint32_t, uint32_t)>& func) {
    if (total_count < 64 || workers_.empty()) {
      func(0, total_count);
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    uint32_t num_threads = static_cast<uint32_t>(workers_.size() + 1);
    uint32_t chunk_size = (total_count + num_threads - 1) / num_threads;

    current_func_ = &func;
    chunk_size_ = chunk_size;
    total_count_ = total_count;
    pending_tasks_.store(static_cast<int>(workers_.size()), std::memory_order_relaxed);
    work_available_ = true;
    generation_++;

    cv_work_.notify_all();
    lock.unlock();

    // Calling thread executes its slice (the last chunk)
    uint32_t main_start = static_cast<uint32_t>(workers_.size()) * chunk_size;
    if (main_start < total_count) {
      func(main_start, total_count);
    }

    // Wait for all workers to finish
    lock.lock();
    cv_done_.wait(lock, [this]() {
      return pending_tasks_.load(std::memory_order_acquire) == 0;
    });
    work_available_ = false;
  }

 private:
  UntileWorkerPool() : running_(true), work_available_(false), generation_(0) {
    unsigned int hw = std::thread::hardware_concurrency();
    uint32_t worker_count = (hw > 1) ? std::min(3u, hw - 1) : 0;
    workers_.reserve(worker_count);
    for (uint32_t i = 0; i < worker_count; ++i) {
      workers_.emplace_back(&UntileWorkerPool::WorkerLoop, this, i);
    }
  }

  ~UntileWorkerPool() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      running_ = false;
      work_available_ = true;
      generation_++;
    }
    cv_work_.notify_all();
    for (auto& w : workers_) {
      if (w.joinable()) {
        w.join();
      }
    }
  }

  void WorkerLoop(uint32_t worker_index) {
    uint64_t last_gen = 0;
    while (true) {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_work_.wait(lock, [this, &last_gen]() {
        return !running_ || (work_available_ && generation_ != last_gen);
      });

      if (!running_) break;

      last_gen = generation_;
      uint32_t chunk = chunk_size_;
      uint32_t total = total_count_;
      const auto* fn = current_func_;
      lock.unlock();

      uint32_t start = worker_index * chunk;
      uint32_t end = std::min(start + chunk, total);
      if (start < end && fn) {
        (*fn)(start, end);
      }

      if (pending_tasks_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        std::lock_guard<std::mutex> d_lock(mutex_);
        cv_done_.notify_one();
      }
    }
  }

  std::vector<std::thread> workers_;
  std::mutex mutex_;
  std::condition_variable cv_work_;
  std::condition_variable cv_done_;
  std::atomic<int> pending_tasks_{0};
  const std::function<void(uint32_t, uint32_t)>* current_func_ = nullptr;
  uint32_t chunk_size_ = 0;
  uint32_t total_count_ = 0;
  uint64_t generation_ = 0;
  bool running_ = true;
  bool work_available_ = false;
};

static void UntileRowRange(uint8_t* output_buffer, const uint8_t* input_buffer,
                           const UntileInfo* untile_info, uint32_t start_y, uint32_t end_y) {
  uint32_t input_bytes_per_block = untile_info->input_format_info->bytes_per_block();
  uint32_t output_bytes_per_block = untile_info->output_format_info->bytes_per_block();
  uint32_t output_pitch = untile_info->output_pitch * output_bytes_per_block;

  // Bytes per pixel
  auto log2_bpp =
      (input_bytes_per_block / 4) + ((input_bytes_per_block / 2) >> (input_bytes_per_block / 4));

  bool is_direct_copy = (untile_info->input_format_info == untile_info->output_format_info);

  // Fast-path: Direct vectorized copy without callback overhead
  if (is_direct_copy) {
    if (output_bytes_per_block == 8) { // DXT1 / BC1 (64-bit blocks)
      uint32_t output_row_offset = start_y * output_pitch;
      for (uint32_t y = start_y; y < end_y; ++y) {
        uint32_t actual_y = untile_info->offset_y + y;
        uint32_t input_row_offset = TiledOffset2DRow(actual_y, untile_info->input_pitch, 3);
        uint8_t* __restrict out_row = output_buffer + output_row_offset;
        uint32_t off_x = untile_info->offset_x;

        for (uint32_t x = 0; x < untile_info->width; ++x) {
          uint32_t in_offset = TiledOffset2DColumn(off_x + x, actual_y, 3, input_row_offset) & ~7u;
          *reinterpret_cast<uint64_t*>(out_row + x * 8) =
              *reinterpret_cast<const uint64_t*>(input_buffer + in_offset);
        }
        output_row_offset += output_pitch;
      }
      return;
    } else if (output_bytes_per_block == 16) { // DXT3, DXT5 / BC2, BC3 (128-bit blocks)
      uint32_t output_row_offset = start_y * output_pitch;
      for (uint32_t y = start_y; y < end_y; ++y) {
        uint32_t actual_y = untile_info->offset_y + y;
        uint32_t input_row_offset = TiledOffset2DRow(actual_y, untile_info->input_pitch, 4);
        uint8_t* __restrict out_row = output_buffer + output_row_offset;
        uint32_t off_x = untile_info->offset_x;

        for (uint32_t x = 0; x < untile_info->width; ++x) {
          uint32_t in_offset = TiledOffset2DColumn(off_x + x, actual_y, 4, input_row_offset) & ~15u;
          const uint64_t* __restrict src = reinterpret_cast<const uint64_t*>(input_buffer + in_offset);
          uint64_t* __restrict dst = reinterpret_cast<uint64_t*>(out_row + x * 16);
          dst[0] = src[0];
          dst[1] = src[1];
        }
        output_row_offset += output_pitch;
      }
      return;
    } else if (output_bytes_per_block == 4) { // RGBA8 / 32-bit colors
      uint32_t output_row_offset = start_y * output_pitch;
      for (uint32_t y = start_y; y < end_y; ++y) {
        uint32_t actual_y = untile_info->offset_y + y;
        uint32_t input_row_offset = TiledOffset2DRow(actual_y, untile_info->input_pitch, 2);
        uint8_t* __restrict out_row = output_buffer + output_row_offset;
        uint32_t off_x = untile_info->offset_x;

        for (uint32_t x = 0; x < untile_info->width; ++x) {
          uint32_t in_offset = TiledOffset2DColumn(off_x + x, actual_y, 2, input_row_offset) & ~3u;
          *reinterpret_cast<uint32_t*>(out_row + x * 4) =
              *reinterpret_cast<const uint32_t*>(input_buffer + in_offset);
        }
        output_row_offset += output_pitch;
      }
      return;
    }
  }

  // Fast-path: CTX1 to R8G8 decoding
  if (untile_info->input_format_info->format == xenos::TextureFormat::k_CTX1 &&
      untile_info->output_format_info->format == xenos::TextureFormat::k_8_8) {
    uint32_t out_pitch_bytes = untile_info->output_pitch * 2;
    for (uint32_t by = start_y; by < end_y; ++by) {
      uint32_t actual_y = untile_info->offset_y + by;
      uint32_t input_row_offset = TiledOffset2DRow(actual_y, untile_info->input_pitch, 3);
      uint32_t off_x = untile_info->offset_x;

      for (uint32_t bx = 0; bx < untile_info->width; ++bx) {
        uint32_t in_offset = TiledOffset2DColumn(off_x + bx, actual_y, 3, input_row_offset) & ~7u;
        const uint8_t* src_blk = input_buffer + in_offset;
        uint8_t g0 = src_blk[0];
        uint8_t r0 = src_blk[1];
        uint8_t g1 = src_blk[2];
        uint8_t r1 = src_blk[3];
        uint32_t xx = *reinterpret_cast<const uint32_t*>(src_blk + 4);

        uint8_t cr[4] = {r0, r1,
                         static_cast<uint8_t>(((2 * r0 + r1) * 85 + 128) >> 8),
                         static_cast<uint8_t>(((r0 + 2 * r1) * 85 + 128) >> 8)};
        uint8_t cg[4] = {g0, g1,
                         static_cast<uint8_t>(((2 * g0 + g1) * 85 + 128) >> 8),
                         static_cast<uint8_t>(((g0 + 2 * g1) * 85 + 128) >> 8)};

        for (uint32_t oy = 0; oy < 4; ++oy) {
          uint32_t row_shift = oy * 8;
          uint8_t* dst_px = output_buffer + (by * 4 + oy) * out_pitch_bytes + (bx * 4 * 2);
          uint16_t* dst16 = reinterpret_cast<uint16_t*>(dst_px);
          dst16[0] = static_cast<uint16_t>(cr[(xx >> (row_shift + 0)) & 3] | (cg[(xx >> (row_shift + 0)) & 3] << 8));
          dst16[1] = static_cast<uint16_t>(cr[(xx >> (row_shift + 2)) & 3] | (cg[(xx >> (row_shift + 2)) & 3] << 8));
          dst16[2] = static_cast<uint16_t>(cr[(xx >> (row_shift + 4)) & 3] | (cg[(xx >> (row_shift + 4)) & 3] << 8));
          dst16[3] = static_cast<uint16_t>(cr[(xx >> (row_shift + 6)) & 3] | (cg[(xx >> (row_shift + 6)) & 3] << 8));
        }
      }
    }
    return;
  }

  // Generic fallback path with format conversion callback
  uint32_t output_row_offset = start_y * output_pitch;
  for (uint32_t y = start_y; y < end_y; ++y) {
    auto input_row_offset =
        TiledOffset2DRow(untile_info->offset_y + y, untile_info->input_pitch, log2_bpp);

    uint32_t output_offset = output_row_offset;
    for (uint32_t x = 0; x < untile_info->width; ++x) {
      auto input_offset = TiledOffset2DColumn(untile_info->offset_x + x, untile_info->offset_y + y,
                                              log2_bpp, input_row_offset);
      input_offset >>= log2_bpp;

      untile_info->copy_callback(&output_buffer[output_offset],
                                 &input_buffer[input_offset * input_bytes_per_block],
                                 output_bytes_per_block);

      output_offset += output_bytes_per_block;
    }

    output_row_offset += output_pitch;
  }
}

void Untile(uint8_t* output_buffer, const uint8_t* input_buffer, const UntileInfo* untile_info) {
  SCOPE_profile_cpu_f("gpu");
  assert_not_null(untile_info);
  assert_not_null(untile_info->input_format_info);
  assert_not_null(untile_info->output_format_info);

  UntileWorkerPool::Instance().ParallelFor(untile_info->height, [&](uint32_t start_y, uint32_t end_y) {
    UntileRowRange(output_buffer, input_buffer, untile_info, start_y, end_y);
  });
}

}  // namespace rex::graphics::texture_conversion
