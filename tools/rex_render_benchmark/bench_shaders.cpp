#include "bench_shaders.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <rex/graphics/pipeline/shader/spirv.h>
#include <rex/graphics/pipeline/shader/spirv_translator.h>
#include <rex/graphics/pipeline/shader/shader.h>
#include <rex/graphics/pipeline/shader/prebaked_shader_cache.h>
#include <rex/graphics/format/ucode.h>
#include <rex/graphics/xenos.h>
#include <rex/string/buffer.h>
#include <xxhash.h>

namespace rex::benchmark {

using namespace rex::graphics;

#pragma pack(push, 1)
struct XeshHeader {
  uint32_t magic;           // 'XESH' = 0x48534558
  uint32_t version_swapped; // byte_swap(0x20201219)
};

struct XeshShaderHeader {
  uint64_t ucode_data_hash;
  uint32_t ucode_dword_count : 31;
  uint32_t type : 1;        // 0 = PS, 1 = VS
};
#pragma pack(pop)

ShaderBenchmarkResult RunShaderBenchmark(const std::string& xsh_path, bool verbose) {
  ShaderBenchmarkResult res;

  std::ifstream f(xsh_path, std::ios::binary);
  if (!f) {
    std::cerr << "[-] [ShaderBench] Nao foi possivel abrir cache XSH em: " << xsh_path << std::endl;
    return res;
  }

  XeshHeader file_header;
  if (!f.read(reinterpret_cast<char*>(&file_header), sizeof(file_header))) {
    std::cerr << "[-] [ShaderBench] Erro ao ler cabecalho XSH" << std::endl;
    return res;
  }

  if (file_header.magic != 0x48534558) {
    std::cerr << "[-] [ShaderBench] Magic invalido no arquivo XSH: 0x" << std::hex
              << file_header.magic << std::dec << std::endl;
    return res;
  }

  // Configurações do tradutor
  SpirvShaderTranslator::Features features_rtv(true);
  features_rtv.fragment_shader_sample_interlock = false;
  SpirvShaderTranslator translator_rtv(features_rtv, false, false, false);

  SpirvShaderTranslator::Features features_rov(true);
  features_rov.fragment_shader_sample_interlock = true;
  SpirvShaderTranslator translator_rov(features_rov, false, false, true);

  rex::string::StringBuffer disasm_buffer;
  std::vector<uint32_t> ucode_buffer;
  ucode_buffer.reserve(0xFFFF);

  std::vector<std::unique_ptr<SpirvShader>> valid_shaders;

  std::cout << "[*] [ShaderBench] Carregando e testando shaders de " << xsh_path << "..." << std::endl;

  while (f) {
    XeshShaderHeader shdr;
    if (!f.read(reinterpret_cast<char*>(&shdr), sizeof(shdr))) break;

    uint32_t dword_count = shdr.ucode_dword_count;
    xenos::ShaderType shader_type = (shdr.type == 1) ? xenos::ShaderType::kVertex : xenos::ShaderType::kPixel;

    ucode_buffer.resize(dword_count);
    size_t byte_count = dword_count * sizeof(uint32_t);
    if (!f.read(reinterpret_cast<char*>(ucode_buffer.data()), byte_count)) break;

    res.total_shaders++;
    res.total_ucode_bytes += byte_count;
    if (shader_type == xenos::ShaderType::kVertex) {
      res.vertex_shaders++;
    } else {
      res.pixel_shaders++;
    }

    std::cout << "[Shader " << res.total_shaders << "/136] 0x" << std::hex << shdr.ucode_data_hash << std::dec << std::flush;

    // 1. Criar SpirvShader
    auto shader = std::make_unique<SpirvShader>(shader_type, shdr.ucode_data_hash,
                                                ucode_buffer.data(), dword_count,
                                                std::endian::big);

    // 2. Medir tempo de ucode analysis
    auto t0 = std::chrono::high_resolution_clock::now();
    disasm_buffer.Reset();
    shader->AnalyzeUcode(disasm_buffer);
    auto t1 = std::chrono::high_resolution_clock::now();
    double analyze_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    res.total_analyze_time_us += analyze_us;
    std::cout << " [ana:" << analyze_us << "us]" << std::flush;

    if (!shader->is_ucode_analyzed()) {
      res.failed_translations++;
      continue;
    }

    // 3. Traduzir para RTV (caminho aproximado / leve)
    uint64_t mod_val_rtv = 0;
    if (shader_type == xenos::ShaderType::kVertex) {
      mod_val_rtv = translator_rtv.GetDefaultVertexShaderModification(0);
    } else {
      mod_val_rtv = translator_rtv.GetDefaultPixelShaderModification(0);
    }

    Shader::Translation* trans_rtv = shader->GetOrCreateTranslation(mod_val_rtv);
    bool translated_ok = false;
    if (trans_rtv) {
      auto t2 = std::chrono::high_resolution_clock::now();
      bool ok_rtv = translator_rtv.TranslateAnalyzedShader(*trans_rtv);
      auto t3 = std::chrono::high_resolution_clock::now();

      double translate_us_rtv = std::chrono::duration<double, std::micro>(t3 - t2).count();
      res.total_translate_time_us_rtv += translate_us_rtv;
      if (translate_us_rtv < res.min_translate_time_us_rtv) res.min_translate_time_us_rtv = translate_us_rtv;
      if (translate_us_rtv > res.max_translate_time_us_rtv) res.max_translate_time_us_rtv = translate_us_rtv;

      if (ok_rtv && trans_rtv->is_valid()) {
        res.successful_translations++;
        res.total_spirv_bytes_rtv += trans_rtv->translated_binary().size();
        translated_ok = true;
      } else {
        res.failed_translations++;
      }
    }

    // 4. Traduzir para ROV (somente se a traducao basica foi bem-sucedida)
    if (translated_ok && shader_type == xenos::ShaderType::kPixel) {
      uint64_t mod_val_rov = translator_rov.GetDefaultPixelShaderModification(0);
      Shader::Translation* trans_rov = shader->GetOrCreateTranslation(mod_val_rov);
      if (trans_rov) {
        auto t4 = std::chrono::high_resolution_clock::now();
        bool ok_rov = translator_rov.TranslateAnalyzedShader(*trans_rov);
        auto t5 = std::chrono::high_resolution_clock::now();

        double translate_us_rov = std::chrono::duration<double, std::micro>(t5 - t4).count();
        res.total_translate_time_us_rov += translate_us_rov;
        if (translate_us_rov < res.min_translate_time_us_rov) res.min_translate_time_us_rov = translate_us_rov;
        if (translate_us_rov > res.max_translate_time_us_rov) res.max_translate_time_us_rov = translate_us_rov;

        if (ok_rov && trans_rov->is_valid()) {
          res.total_spirv_bytes_rov += trans_rov->translated_binary().size();
        }
      }
    }

    std::cout << " [rtv:" << (translated_ok ? "ok" : "fail") << "]" << std::endl;

    if (translated_ok) {
      valid_shaders.push_back(std::move(shader));
    }
  }

  std::cout << "[+] [ShaderBench] Loop de shaders concluido. Total validos: " << valid_shaders.size() << std::endl;

  // 5. Teste de AOT Cache / Lookup em tempo quente (Hot cache lookup)
  if (!valid_shaders.empty()) {
    std::cout << "[+] [ShaderBench] Step 5: Testando lookup em cache quente..." << std::endl;
    auto t_cache0 = std::chrono::high_resolution_clock::now();
    constexpr size_t kLookupLoops = 10000;
    for (size_t i = 0; i < kLookupLoops; ++i) {
      size_t idx = i % valid_shaders.size();
      uint64_t mod = (valid_shaders[idx]->type() == xenos::ShaderType::kVertex)
                         ? translator_rtv.GetDefaultVertexShaderModification(0)
                         : translator_rtv.GetDefaultPixelShaderModification(0);
      Shader::Translation* cached = valid_shaders[idx]->GetOrCreateTranslation(mod);
      (void)cached;
    }
    auto t_cache1 = std::chrono::high_resolution_clock::now();
    double total_ns = std::chrono::duration<double, std::nano>(t_cache1 - t_cache0).count();
    res.cache_hit_lookup_time_ns = total_ns / kLookupLoops;
  }

  if (res.total_shaders > 0) {
    res.avg_analyze_time_us = res.total_analyze_time_us / res.total_shaders;
    if (res.successful_translations > 0) {
      res.avg_translate_time_us_rtv = res.total_translate_time_us_rtv / res.successful_translations;
    }
    if (res.pixel_shaders > 0) {
      res.avg_translate_time_us_rov = res.total_translate_time_us_rov / res.pixel_shaders;
    }
    if (res.cache_hit_lookup_time_ns > 0.0) {
      double avg_translate_ns = res.avg_translate_time_us_rtv * 1000.0;
      res.aot_cache_speedup_x = avg_translate_ns / res.cache_hit_lookup_time_ns;
    }
  }

  return res;
}

bool BakeAOTShaders(const std::string& xsh_path, const std::string& output_cache_path) {
  std::ifstream f(xsh_path, std::ios::binary);
  if (!f) {
    std::cerr << "[!] Erro ao abrir arquivo XSH: " << xsh_path << std::endl;
    return false;
  }

  XeshHeader file_header;
  if (!f.read(reinterpret_cast<char*>(&file_header), sizeof(file_header))) {
    std::cerr << "[!] Arquivo XSH invalido ou truncado." << std::endl;
    return false;
  }

  SpirvShaderTranslator::Features features_rtv(true);
  features_rtv.spirv_version = spv::Spv_1_3;
  features_rtv.max_storage_buffer_range = UINT32_MAX;
  features_rtv.clip_distance = true;
  features_rtv.cull_distance = true;
  features_rtv.full_draw_index_uint32 = true;
  SpirvShaderTranslator translator_rtv(features_rtv, false, false, false);

  PrebakedShaderCache cache;
  rex::string::StringBuffer disasm_buffer;
  std::vector<uint32_t> ucode_buffer;

  uint32_t total = 0;
  uint32_t baked = 0;

  std::cout << "[*] [AOT Baker] Compilando offline todos os shaders de " << xsh_path << "..." << std::endl;

  while (f) {
    XeshShaderHeader shdr;
    if (!f.read(reinterpret_cast<char*>(&shdr), sizeof(shdr))) break;

    uint32_t dword_count = shdr.ucode_dword_count;
    xenos::ShaderType shader_type = (shdr.type == 1) ? xenos::ShaderType::kVertex : xenos::ShaderType::kPixel;

    ucode_buffer.resize(dword_count);
    size_t byte_count = dword_count * sizeof(uint32_t);
    if (!f.read(reinterpret_cast<char*>(ucode_buffer.data()), byte_count)) break;

    total++;
    auto shader = std::make_unique<SpirvShader>(shader_type, shdr.ucode_data_hash,
                                                ucode_buffer.data(), dword_count,
                                                std::endian::big);

    disasm_buffer.Reset();
    shader->AnalyzeUcode(disasm_buffer);
    if (!shader->is_ucode_analyzed()) {
      continue;
    }

    uint64_t mod_val = 0;
    if (shader_type == xenos::ShaderType::kVertex) {
      mod_val = translator_rtv.GetDefaultVertexShaderModification(0);
    } else {
      mod_val = translator_rtv.GetDefaultPixelShaderModification(0);
    }

    Shader::Translation* trans = shader->GetOrCreateTranslation(mod_val);
    if (!trans) {
      continue;
    }

    if (!translator_rtv.TranslateAnalyzedShader(*trans) || !trans->is_valid()) {
      continue;
    }

    const auto& spv = trans->translated_binary();
    if (spv.empty()) {
      continue;
    }

    PrebakedShaderEntry entry;
    entry.ucode_hash = shdr.ucode_data_hash;
    entry.shader_type = shader_type;
    entry.texture_bindings = shader->GetTextureBindingsAfterTranslation();
    entry.sampler_bindings = shader->GetSamplerBindingsAfterTranslation();
    entry.spirv_binary = spv;

    cache.AddShader(std::move(entry));
    baked++;
  }

  std::cout << "[+] [AOT Baker] Shaders pre-compilados com sucesso: " << baked << " / " << total << std::endl;
  return cache.SaveToFile(output_cache_path);
}

}  // namespace rex::benchmark
