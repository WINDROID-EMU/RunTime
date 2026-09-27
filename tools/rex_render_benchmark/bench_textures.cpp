#include "bench_textures.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <numeric>
#include <vector>

#include <rex/graphics/pipeline/texture/conversion.h>
#include <rex/graphics/pipeline/texture/util.h>
#include <rex/graphics/xenos.h>

namespace rex::benchmark {

namespace {

void Untile2D_Benchmark(const uint8_t* __restrict src, uint8_t* __restrict dst,
                        uint32_t width_texels, uint32_t height_texels,
                        uint32_t block_width, uint32_t block_height,
                        uint32_t bytes_per_block, uint32_t bytes_per_block_log2) {
  rex::graphics::xenos::TextureFormat format = rex::graphics::xenos::TextureFormat::k_8_8_8_8;
  if (bytes_per_block == 8) format = rex::graphics::xenos::TextureFormat::k_DXT1;
  else if (bytes_per_block == 16) format = rex::graphics::xenos::TextureFormat::k_DXT4_5;
  else if (bytes_per_block == 4) format = rex::graphics::xenos::TextureFormat::k_8_8_8_8;

  const auto* fmt_info = rex::graphics::FormatInfo::Get(format);
  uint32_t blocks_x = (width_texels + block_width - 1) / block_width;
  uint32_t blocks_y = (height_texels + block_height - 1) / block_height;
  uint32_t pitch_blocks = (blocks_x + 31) & ~31;

  rex::graphics::texture_conversion::UntileInfo untile_info;
  untile_info.input_format_info = fmt_info;
  untile_info.output_format_info = fmt_info;
  untile_info.width = blocks_x;
  untile_info.height = blocks_y;
  untile_info.offset_x = 0;
  untile_info.offset_y = 0;
  untile_info.input_pitch = pitch_blocks;
  untile_info.output_pitch = blocks_x;
  untile_info.copy_callback = [](void* d, const void* s, size_t sz) {
    std::memcpy(d, s, sz);
  };

  rex::graphics::texture_conversion::Untile(dst, src, &untile_info);
}

}  // namespace

TextureBenchmarkResult RunTextureBenchmark(bool verbose) {
  TextureBenchmarkResult res;

  std::cout << "[*] [TextureBench] Executando benchmarks de untiling e conversao de texturas..." << std::endl;

  struct Config {
    std::string name;
    std::string format;
    uint32_t width;
    uint32_t height;
    uint32_t block_w;
    uint32_t block_h;
    uint32_t block_bytes;
    uint32_t block_bytes_log2;
    uint32_t iterations;
  };

  std::vector<Config> configs = {
      {"Menu/UI Small", "DXT1", 256, 256, 4, 4, 8, 3, 50},
      {"Vehicle/Car Body", "DXT1", 512, 512, 4, 4, 8, 3, 20},
      {"World Road/Track", "DXT1", 1024, 1024, 4, 4, 8, 3, 10},
      {"World Sky/EnvMap", "DXT1", 2048, 2048, 4, 4, 8, 3, 4},
      {"Vehicle Interior", "DXT5", 512, 512, 4, 4, 16, 4, 20},
      {"World Building Atlas", "DXT5", 1024, 1024, 4, 4, 16, 4, 10},
      {"HUD / Minimap", "RGBA8", 512, 512, 1, 1, 4, 2, 20},
      {"Fullscreen Framebuffer", "RGBA8", 1280, 720, 1, 1, 4, 2, 10},
  };

  double total_time_acc = 0.0;
  double total_throughput_acc = 0.0;

  for (const auto& cfg : configs) {
    uint32_t blocks_x = (cfg.width + cfg.block_w - 1) / cfg.block_w;
    uint32_t blocks_y = (cfg.height + cfg.block_h - 1) / cfg.block_h;
    uint32_t pitch_blocks = (blocks_x + 31) & ~31;
    uint32_t pitch_rows = (blocks_y + 31) & ~31;

    size_t tiled_size = static_cast<size_t>(pitch_blocks) * pitch_rows * cfg.block_bytes + 4096;
    size_t linear_size = static_cast<size_t>(blocks_x) * blocks_y * cfg.block_bytes;

    std::vector<uint8_t> src(tiled_size, 0xAA);
    std::vector<uint8_t> dst(linear_size, 0);

    // Warmup
    Untile2D_Benchmark(src.data(), dst.data(), cfg.width, cfg.height, cfg.block_w, cfg.block_h,
                       cfg.block_bytes, cfg.block_bytes_log2);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (uint32_t it = 0; it < cfg.iterations; ++it) {
      Untile2D_Benchmark(src.data(), dst.data(), cfg.width, cfg.height, cfg.block_w, cfg.block_h,
                         cfg.block_bytes, cfg.block_bytes_log2);
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    double total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double avg_us = total_us / cfg.iterations;
    double throughput_mb_s = (static_cast<double>(linear_size) / (1024.0 * 1024.0)) / (avg_us / 1e6);

    total_time_acc += (avg_us / 1000.0);
    total_throughput_acc += throughput_mb_s;

    TextureBenchmarkResult::CaseResult cr;
    cr.name = cfg.name;
    cr.format = cfg.format;
    cr.width = cfg.width;
    cr.height = cfg.height;
    cr.size_bytes = linear_size;
    cr.untile_time_us = avg_us;
    cr.throughput_mb_s = throughput_mb_s;

    res.cases.push_back(cr);

    if (verbose) {
      std::cout << "  - " << cfg.name << " (" << cfg.format << " " << cfg.width << "x" << cfg.height
                << "): " << avg_us << " us, " << throughput_mb_s << " MB/s" << std::endl;
    }
  }

  res.total_time_ms = total_time_acc;
  res.avg_throughput_mb_s = total_throughput_acc / configs.size();

  // Teste de conversao de formato: CTX1 (Normal Map) -> R8G8
  {
    constexpr uint32_t kTexW = 512;
    constexpr uint32_t kTexH = 512;
    constexpr uint32_t kBlocksX = kTexW / 4;
    constexpr uint32_t kBlocksY = kTexH / 4;
    constexpr size_t kCtx1Size = kBlocksX * kBlocksY * 8; // 128 KB
    constexpr uint32_t kRowPitch = kTexW * 2;             // 1024 bytes
    constexpr size_t kR8G8Size = kTexH * kRowPitch;       // 512 KB

    std::vector<uint8_t> ctx1_src(kCtx1Size, 0x55);
    std::vector<uint8_t> r8g8_dst(kR8G8Size, 0);

    rex::graphics::texture_conversion::UntileInfo untile_info;
    untile_info.input_format_info = rex::graphics::FormatInfo::Get(rex::graphics::xenos::TextureFormat::k_CTX1);
    untile_info.output_format_info = rex::graphics::FormatInfo::Get(rex::graphics::xenos::TextureFormat::k_8_8);
    untile_info.width = kBlocksX;
    untile_info.height = kBlocksY;
    untile_info.offset_x = 0;
    untile_info.offset_y = 0;
    untile_info.input_pitch = kBlocksX;
    untile_info.output_pitch = kTexW;
    untile_info.copy_callback = [](void*, const void*, size_t) {};

    // Warmup
    rex::graphics::texture_conversion::Untile(r8g8_dst.data(), ctx1_src.data(), &untile_info);

    auto t0 = std::chrono::high_resolution_clock::now();
    constexpr int kConvIters = 20;
    for (int it = 0; it < kConvIters; ++it) {
      rex::graphics::texture_conversion::Untile(r8g8_dst.data(), ctx1_src.data(), &untile_info);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double avg_sec = (total_us / kConvIters) / 1e6;
    res.ctx1_throughput_mb_s = (static_cast<double>(kCtx1Size) / (1024.0 * 1024.0)) / avg_sec;
  }

  // Teste de swap de endianness
  {
    constexpr size_t kSwapTestSize = 4 * 1024 * 1024; // 4MB
    std::vector<uint8_t> swap_src(kSwapTestSize, 0x12);
    std::vector<uint8_t> swap_dst(kSwapTestSize, 0);

    auto t0 = std::chrono::high_resolution_clock::now();
    constexpr int kSwapIters = 20;
    for (int i = 0; i < kSwapIters; ++i) {
      rex::graphics::texture_conversion::CopySwapBlock(
          rex::graphics::xenos::Endian::k8in32, swap_dst.data(), swap_src.data(), kSwapTestSize);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double avg_sec = (total_us / kSwapIters) / 1e6;
    res.endian_swap_throughput_mb_s = (static_cast<double>(kSwapTestSize) / (1024.0 * 1024.0)) / avg_sec;
  }

  return res;
}

}  // namespace rex::benchmark
