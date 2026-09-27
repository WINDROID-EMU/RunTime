#pragma once

#include <cstdint>
#include <string>

namespace rex::benchmark {

struct DispatchBenchmarkResult {
  uint32_t simulated_packets = 0;
  uint32_t simulated_draws = 0;
  uint32_t simulated_swaps = 0;
  uint32_t state_packets = 0;
  uint32_t shader_packets = 0;
  double dispatch_time_us = 0.0;
  double packets_per_sec = 0.0;
  double time_per_draw_ns = 0.0;
  double time_per_packet_ns = 0.0;
};

DispatchBenchmarkResult RunDispatchBenchmark(uint32_t num_frames, bool verbose);

}  // namespace rex::benchmark
