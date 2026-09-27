#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rex::benchmark {

struct ShaderBenchmarkResult {
  size_t total_shaders = 0;
  size_t vertex_shaders = 0;
  size_t pixel_shaders = 0;
  size_t successful_translations = 0;
  size_t failed_translations = 0;

  double total_analyze_time_us = 0.0;
  double avg_analyze_time_us = 0.0;

  double total_translate_time_us_rtv = 0.0;
  double avg_translate_time_us_rtv = 0.0;
  double min_translate_time_us_rtv = 1e9;
  double max_translate_time_us_rtv = 0.0;

  double total_translate_time_us_rov = 0.0;
  double avg_translate_time_us_rov = 0.0;
  double min_translate_time_us_rov = 1e9;
  double max_translate_time_us_rov = 0.0;

  double cache_hit_lookup_time_ns = 0.0;
  double aot_cache_speedup_x = 0.0;

  size_t total_ucode_bytes = 0;
  size_t total_spirv_bytes_rtv = 0;
  size_t total_spirv_bytes_rov = 0;
};

ShaderBenchmarkResult RunShaderBenchmark(const std::string& xsh_path, bool verbose);

bool BakeAOTShaders(const std::string& xsh_path, const std::string& output_cache_path);

}  // namespace rex::benchmark
