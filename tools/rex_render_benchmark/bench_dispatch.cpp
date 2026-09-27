#include "bench_dispatch.h"

#include <chrono>
#include <iostream>

#include <rex/graphics/pm4_plume_transpiler.h>

namespace rex::benchmark {

DispatchBenchmarkResult RunDispatchBenchmark(uint32_t num_frames, bool verbose) {
  DispatchBenchmarkResult res;

  std::cout << "[*] [DispatchBench] Simulando transmissao de pacotes PM4 e chamadas de draw..." << std::endl;

  rex::graphics::Pm4PlumeTranspiler transpiler;
  transpiler.SetEnabled(true);

  // Um frame tipico do NFSMW contem aproximadamente:
  // - 1500 pacotes PM4
  // - 250 draw calls
  // - 800 gravacoes de constantes / estado
  // - 20 cargas de shaders
  // - 1 XE_SWAP
  constexpr uint32_t kDrawsPerFrame = 250;
  constexpr uint32_t kStatesPerDraw = 3;

  auto t0 = std::chrono::high_resolution_clock::now();

  for (uint32_t frame = 0; frame < num_frames; ++frame) {
    for (uint32_t d = 0; d < kDrawsPerFrame; ++d) {
      // 1. Estados e Constantes
      for (uint32_t s = 0; s < kStatesPerDraw; ++s) {
        transpiler.ObservePacket(0x2D, 4); // PM4_SET_CONSTANT
        transpiler.ObserveRegisterWrite(0x2000 + (s * 4), 0x3F800000 + d + s);
        res.state_packets++;
        res.simulated_packets++;
      }

      // 2. Mudanca de Shader ocasional
      if (d % 15 == 0) {
        transpiler.ObservePacket(0x27, 2); // PM4_IM_LOAD
        transpiler.ObserveShaderLoad(0, 0x82000000 + (d * 0x100), 128);
        res.shader_packets++;
        res.simulated_packets++;
      }

      // 3. Draw call
      transpiler.ObservePacket(0x22, 5); // PM4_DRAW_INDX
      transpiler.ObserveDraw("DRAW_INDX", 4 /* Triangles */, 1024, true);
      res.simulated_draws++;
      res.simulated_packets++;
    }

    // 4. Fim de frame
    transpiler.ObservePacket(0x64, 1); // PM4_XE_SWAP
    res.simulated_swaps++;
    res.simulated_packets++;
  }

  auto t1 = std::chrono::high_resolution_clock::now();
  double total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
  res.dispatch_time_us = total_us;

  if (total_us > 0.0) {
    double sec = total_us / 1e6;
    res.packets_per_sec = res.simulated_packets / sec;
    res.time_per_draw_ns = (total_us * 1000.0) / res.simulated_draws;
    res.time_per_packet_ns = (total_us * 1000.0) / res.simulated_packets;
  }

  if (verbose) {
    std::cout << "  - Pacotes processados: " << transpiler.stats().packets_seen << std::endl;
    std::cout << "  - Draws processados: " << transpiler.stats().draw_packets << std::endl;
    std::cout << "  - Swaps: " << transpiler.stats().swap_packets << std::endl;
  }

  return res;
}

}  // namespace rex::benchmark
