#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "bench_shaders.h"
#include "bench_textures.h"
#include "bench_dispatch.h"

namespace fs = std::filesystem;

void PrintBanner() {
  std::cout << "================================================================================" << std::endl;
  std::cout << "        ReXGlue Runtime - Render & Pipeline Graphics Benchmark Suite           " << std::endl;
  std::cout << "   Analise de Desempenho: Shaders Xenos, Conversao de Texturas e Dispatch PM4   " << std::endl;
  std::cout << "================================================================================" << std::endl;
}

std::string FindDefaultXsh() {
  const std::vector<std::string> candidates = {
      "/data/local/tmp/nfsmw_bench/454107D9_completo.xsh",
      "/media/windroid/SSD KING/PROJETO NFSMW RECOMP/Migração/454107D9_completo.xsh",
      "/media/windroid/SSD KING/PROJETO NFSMW RECOMP/NFSMW-RECOMP/android/app/src/main/assets/shaders/shareable/454107D9.xsh",
      "454107D9_completo.xsh",
      "454107D9.xsh",
      "../Migração/454107D9_completo.xsh",
  };
  for (const auto& path : candidates) {
    if (fs::exists(path)) {
      return path;
    }
  }
  return "";
}

int main(int argc, char** argv) {
  PrintBanner();

  std::string xsh_path = FindDefaultXsh();
  uint32_t simulated_frames = 200;
  bool verbose = false;
  std::string bake_aot_path = "";
  std::string json_output_path = "";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--xsh" && i + 1 < argc) {
      xsh_path = argv[++i];
    } else if (arg == "--bake-aot" && i + 1 < argc) {
      bake_aot_path = argv[++i];
    } else if (arg == "--frames" && i + 1 < argc) {
      simulated_frames = static_cast<uint32_t>(std::atoi(argv[++i]));
    } else if (arg == "--json" && i + 1 < argc) {
      json_output_path = argv[++i];
    } else if (arg == "--verbose" || arg == "-v") {
      verbose = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Uso: " << argv[0] << " [opcoes]\n\n"
                << "Opcoes:\n"
                << "  --xsh <path>       Caminho para o cache .xsh de shaders do jogo\n"
                << "  --bake-aot <path>  Compila e salva os shaders AOT em um arquivo .spvcache\n"
                << "  --frames <num>     Numero de frames simulados no teste PM4 (padrao: 200)\n"
                << "  --json <path>      Salvar relatorio detalhado em formato JSON\n"
                << "  --verbose, -v      Exibir informacoes detalhadas de cada shader/textura\n"
                << "  --help, -h         Exibe esta mensagem de ajuda\n";
      return 0;
    }
  }

  if (!bake_aot_path.empty()) {
    std::cout << "[*] Modo AOT Shader Baker selecionado.\n";
    if (xsh_path.empty() || !fs::exists(xsh_path)) {
      std::cerr << "[!] Erro: Arquivo XSH nao especificado ou nao encontrado: " << xsh_path << std::endl;
      return 1;
    }
    bool ok = rex::benchmark::BakeAOTShaders(xsh_path, bake_aot_path);
    if (ok) {
      std::cout << "[✓] Cache AOT gerado com sucesso em: " << bake_aot_path << std::endl;
      return 0;
    } else {
      std::cerr << "[X] Falha ao gerar cache AOT.\n";
      return 1;
    }
  }

  std::cout << "[*] Arquivo de Shaders XSH: " << (xsh_path.empty() ? "(nao encontrado)" : xsh_path) << std::endl;
  std::cout << "[*] Frames Simulados PM4:  " << simulated_frames << std::endl;
  std::cout << "--------------------------------------------------------------------------------" << std::endl;

  // 1. Benchmark de Shaders
  rex::benchmark::ShaderBenchmarkResult shader_res;
  if (!xsh_path.empty() && fs::exists(xsh_path)) {
    shader_res = rex::benchmark::RunShaderBenchmark(xsh_path, verbose);
  } else {
    std::cerr << "[!] Aviso: Nenhum cache .xsh encontrado. O teste de shaders sera ignorado.\n"
              << "    Forneca com: --xsh <caminho_para_454107D9.xsh>\n";
  }
  std::cout << "[+] [Main] Benchmark de shaders finalizado." << std::endl;

  // 2. Benchmark de Texturas
  rex::benchmark::TextureBenchmarkResult tex_res = rex::benchmark::RunTextureBenchmark(verbose);
  std::cout << "[+] [Main] Benchmark de texturas finalizado." << std::endl;

  // 3. Benchmark de PM4 Dispatch
  rex::benchmark::DispatchBenchmarkResult dispatch_res =
      rex::benchmark::RunDispatchBenchmark(simulated_frames, verbose);
  std::cout << "[+] [Main] Benchmark de PM4 dispatch finalizado." << std::endl;

  std::cout << "\n================================================================================" << std::endl;
  std::cout << "                       RELATORIO DE RESULTADOS E GARGALOS                       " << std::endl;
  std::cout << "================================================================================" << std::endl;

  // Secao 1: Shaders
  std::cout << "\n[1] PIPELINE DE SHADERS (Xenos uCode -> SPIR-V):" << std::endl;
  if (shader_res.total_shaders > 0) {
    std::cout << "  - Total de shaders analisados:   " << shader_res.total_shaders
              << " (VS: " << shader_res.vertex_shaders << ", PS: " << shader_res.pixel_shaders << ")\n"
              << "  - Traduzidos com sucesso:        " << shader_res.successful_translations << " / "
              << shader_res.total_shaders << " ("
              << (shader_res.successful_translations * 100.0 / shader_res.total_shaders) << "%)\n"
              << "  - Tempo medio de analise uCode:  " << std::fixed << std::setprecision(2)
              << shader_res.avg_analyze_time_us << " us / shader\n"
              << "  - Traducao Fria RTV (Aproximada):" << std::setprecision(2)
              << shader_res.avg_translate_time_us_rtv << " us / shader (Total: "
              << (shader_res.total_translate_time_us_rtv / 1000.0) << " ms)\n"
              << "  - Traducao Fria ROV (Interlock): " << std::setprecision(2)
              << shader_res.avg_translate_time_us_rov << " us / shader (Total: "
              << (shader_res.total_translate_time_us_rov / 1000.0) << " ms)\n"
              << "  - Lookup Quente em Cache (AOT):  " << std::setprecision(2)
              << shader_res.cache_hit_lookup_time_ns << " ns / shader\n"
              << "  - ACELERACAO COM CACHE AOT/XSH:  " << std::setprecision(1)
              << shader_res.aot_cache_speedup_x << "x MAIS RAPIDO (ZERO HITCHING)\n"
              << "  - Sobrecarga de Tamanho ROV/RTV: "
              << (shader_res.total_spirv_bytes_rtv > 0
                      ? ((double)shader_res.total_spirv_bytes_rov / shader_res.total_spirv_bytes_rtv * 100.0)
                      : 0.0)
              << "% em relacao ao caminho direto\n";
  } else {
    std::cout << "  - Nao executado (arquivo .xsh nao fornecido).\n";
  }

  // Secao 2: Texturas
  std::cout << "\n[2] PROCESSAMENTO E CONVERSAO DE TEXTURAS:" << std::endl;
  std::cout << "  - Vazao media de untiling CPU:   " << std::fixed << std::setprecision(1)
            << tex_res.avg_throughput_mb_s << " MB/s\n"
            << "  - Conversao CTX1 -> R8G8:        " << std::setprecision(1)
            << tex_res.ctx1_throughput_mb_s << " MB/s\n"
            << "  - Swap Endianness (8-in-32):     " << std::setprecision(1)
            << tex_res.endian_swap_throughput_mb_s << " MB/s\n";
  std::cout << "  - Latencia por resolucao (Software CPU Untiling):\n";
  for (const auto& c : tex_res.cases) {
    std::cout << "    * " << std::left << std::setw(24) << c.name << " (" << std::setw(5) << c.format
              << " " << std::setw(9) << (std::to_string(c.width) + "x" + std::to_string(c.height))
              << "): " << std::right << std::setw(7) << std::setprecision(2) << (c.untile_time_us / 1000.0)
              << " ms  [" << std::setw(7) << std::setprecision(1) << c.throughput_mb_s << " MB/s]\n";
  }

  // Secao 3: Dispatch PM4
  std::cout << "\n[3] DESPACHO DE COMANDOS GRAFICOS (PM4 / Plume):" << std::endl;
  std::cout << "  - Pacotes simulados:             " << dispatch_res.simulated_packets << "\n"
            << "  - Draw calls simulados:          " << dispatch_res.simulated_draws << "\n"
            << "  - Taxa de despacho PM4:          " << std::fixed << std::setprecision(2)
            << (dispatch_res.packets_per_sec / 1e6) << " Milhoes de pacotes / seg\n"
            << "  - Custo de CPU por Draw Call:    " << std::setprecision(1)
            << dispatch_res.time_per_draw_ns << " ns / draw\n"
            << "  - Custo de CPU por Pacote PM4:   " << std::setprecision(1)
            << dispatch_res.time_per_packet_ns << " ns / pacote\n";

  // Secao 4: Diagnostico Executivo de Gargalos
  std::cout << "\n================================================================================" << std::endl;
  std::cout << "                   DIAGNOSTICO EXECUTIVO E PLANO DE OTIMIZACAO                  " << std::endl;
  std::cout << "================================================================================" << std::endl;

  std::cout << "\n[GARGALO 1: Compilacao Dinamica de Shaders]\n"
            << "  Evidencia: Traduzir os " << shader_res.total_shaders << " shaders em runtime consome "
            << std::fixed << std::setprecision(1) << (shader_res.total_translate_time_us_rtv / 1000.0)
            << " ms de CPU.\n"
            << "  Impacto:   Travamentos e micro-stutters cada vez que um shader novo e ativado no jogo.\n"
            << "  Solucao:   Recompilacao Ahead-Of-Time (AOT) ou empacotamento do cache 454107D9.xsh\n"
            << "             diretamente no build/APK. O lookup com cache cai para "
            << std::setprecision(1) << shader_res.cache_hit_lookup_time_ns << " ns (ganho de "
            << std::setprecision(0) << shader_res.aot_cache_speedup_x << "x).\n";

  std::cout << "\n[GARGALO 2: Emulacao de EDRAM via Fragment Shader Interlock (ROV)]\n"
            << "  Evidencia: O modo ROV expande o tamanho e complexidade do shader em relacao ao modo RTV.\n"
            << "  Impacto:   Em GPUs mobile (Adreno/Turnip), o interlock forca execucao em serie de pixels,\n"
            << "             derrubando a taxa de quadros para ~3 FPS em 3D.\n"
            << "  Solucao:   Utilizar o caminho RTV aproximado ou Dynamic Rendering do Vulkan/Plume,\n"
            << "             permitindo que os ROPs de hardware executem o depth test e blending nativo.\n";

  std::cout << "\n[GARGALO 3: Untiling de Texturas na CPU]\n"
            << "  Evidencia: Texturas 1024x1024 e 2048x2048 levam de "
            << std::setprecision(2) << (tex_res.cases.empty() ? 0.0 : tex_res.cases[2].untile_time_us / 1000.0)
            << " ms a "
            << std::setprecision(2) << (tex_res.cases.size() > 3 ? tex_res.cases[3].untile_time_us / 1000.0 : 0.0)
            << " ms CADA na CPU.\n"
            << "  Impacto:   Em um orcamento de 16.6 ms (60 FPS), carregar 5 texturas na CPU estoura o frame.\n"
            << "  Solucao:   Migrar o untiling para os Compute Shaders da GPU (PlumeTextureUploader já\n"
            << "             disponivel em graphics_plume) ou pré-processar as texturas offline.\n";

  std::cout << "\n[GARGALO 4: Despacho de Comandos e Bridge PM4]\n"
            << "  Evidencia: O pipeline de observacao transposta atinge "
            << std::setprecision(1) << (dispatch_res.packets_per_sec / 1e6) << " Mpacotes/s com apenas "
            << std::setprecision(0) << dispatch_res.time_per_draw_ns << " ns por draw call.\n"
            << "  Conclusao: A CPU NAO e o gargalo no despacho PM4; o estrangulamento esta 100% concentrado\n"
            << "             na compilacao dinamica de shaders e na emulacao pesada de EDRAM/texturas.\n";

  std::cout << "================================================================================\n" << std::endl;

  // Salvar JSON se solicitado
  if (!json_output_path.empty()) {
    std::ofstream jf(json_output_path);
    if (jf) {
      jf << "{\n"
         << "  \"shaders\": {\n"
         << "    \"total\": " << shader_res.total_shaders << ",\n"
         << "    \"vertex\": " << shader_res.vertex_shaders << ",\n"
         << "    \"pixel\": " << shader_res.pixel_shaders << ",\n"
         << "    \"successful\": " << shader_res.successful_translations << ",\n"
         << "    \"avg_translate_us_rtv\": " << shader_res.avg_translate_time_us_rtv << ",\n"
         << "    \"total_translate_ms_rtv\": " << (shader_res.total_translate_time_us_rtv / 1000.0) << ",\n"
         << "    \"cache_lookup_ns\": " << shader_res.cache_hit_lookup_time_ns << ",\n"
         << "    \"cache_speedup_x\": " << shader_res.aot_cache_speedup_x << "\n"
         << "  },\n"
         << "  \"textures\": {\n"
         << "    \"avg_throughput_mb_s\": " << tex_res.avg_throughput_mb_s << ",\n"
         << "    \"ctx1_throughput_mb_s\": " << tex_res.ctx1_throughput_mb_s << ",\n"
         << "    \"endian_swap_throughput_mb_s\": " << tex_res.endian_swap_throughput_mb_s << "\n"
         << "  },\n"
         << "  \"dispatch\": {\n"
         << "    \"packets_per_sec\": " << dispatch_res.packets_per_sec << ",\n"
         << "    \"time_per_draw_ns\": " << dispatch_res.time_per_draw_ns << "\n"
         << "  }\n"
         << "}\n";
      std::cout << "[✓] Relatorio JSON salvo com sucesso em: " << json_output_path << std::endl;
    }
  }

  return 0;
}
