#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rex::benchmark {

struct TextureBenchmarkResult {
  struct CaseResult {
    std::string name;
    std::string format;
    uint32_t width = 0;
    uint32_t height = 0;
    size_t size_bytes = 0;
    double untile_time_us = 0.0;
    double throughput_mb_s = 0.0;
  };

  std::vector<CaseResult> cases;
  double total_time_ms = 0.0;
  double avg_throughput_mb_s = 0.0;

  double ctx1_throughput_mb_s = 0.0;
  double endian_swap_throughput_mb_s = 0.0;
};

TextureBenchmarkResult RunTextureBenchmark(bool verbose);

}  // namespace rex::benchmark
