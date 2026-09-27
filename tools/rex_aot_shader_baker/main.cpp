#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include <rex/graphics/format/ucode.h>
#include <rex/graphics/pipeline/shader/prebaked_shader_cache.h>
#include <rex/graphics/pipeline/shader/spirv.h>
#include <rex/graphics/pipeline/shader/spirv_translator.h>
#include <rex/graphics/xenos.h>
#include <xxhash.h>

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

int main(int argc, char* argv[]) {
  std::string xsh_path = "/media/windroid/SSD KING/PROJETO NFSMW RECOMP/Migração/454107D9_completo.xsh";
  std::string output_cache = "454107D9_prebaked.spvcache";
  std::string header_output = "";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--xsh" && i + 1 < argc) {
      xsh_path = argv[++i];
    } else if (arg == "--output" && i + 1 < argc) {
      output_cache = argv[++i];
    } else if (arg == "--header" && i + 1 < argc) {
      header_output = argv[++i];
    }
  }

  std::cout << "========================================================" << std::endl;
  std::cout << "      ReXGlue AOT Shader Baker (Offline Pre-compiler)   " << std::endl;
  std::cout << "========================================================" << std::endl;
  std::cout << "Entrada XSH: " << xsh_path << std::endl;
  std::cout << "Saida Cache: " << output_cache << std::endl;

  std::ifstream f(xsh_path, std::ios::binary);
  if (!f) {
    std::cerr << "ERRO: Nao foi possivel abrir o arquivo .xsh: " << xsh_path << std::endl;
    return 1;
  }

  XeshHeader file_header;
  if (!f.read(reinterpret_cast<char*>(&file_header), sizeof(file_header))) {
    std::cerr << "ERRO: Nao foi possivel ler o cabecalho XSH: " << xsh_path << std::endl;
    return 1;
  }
  if (file_header.magic != 0x48534558) {
    std::cerr << "ERRO: Magic XSH invalido: 0x" << std::hex << file_header.magic << std::dec << std::endl;
    return 1;
  }

  SpirvShaderTranslator::Features features_rtv(false);
  features_rtv.spirv_version = spv::Spv_1_3;
  features_rtv.max_storage_buffer_range = UINT32_MAX;
  features_rtv.clip_distance = true;
  features_rtv.cull_distance = true;
  features_rtv.full_draw_index_uint32 = true;
  SpirvShaderTranslator translator(features_rtv, false, false, false);

  PrebakedShaderCache cache;
  rex::string::StringBuffer disasm_buffer;
  std::vector<uint32_t> ucode_buffer;

  uint32_t count = 0;
  uint32_t success = 0;
  auto t_start = std::chrono::high_resolution_clock::now();

  while (f) {
    XeshShaderHeader shdr;
    if (!f.read(reinterpret_cast<char*>(&shdr), sizeof(shdr))) break;

    uint32_t dword_count = shdr.ucode_dword_count;
    xenos::ShaderType shader_type = (shdr.type == 1) ? xenos::ShaderType::kVertex : xenos::ShaderType::kPixel;

    ucode_buffer.resize(dword_count);
    size_t byte_count = dword_count * sizeof(uint32_t);
    if (!f.read(reinterpret_cast<char*>(ucode_buffer.data()), byte_count)) break;

    count++;
    std::cout << "[" << count << "/136] Shader 0x" << std::hex << shdr.ucode_data_hash << std::dec
              << " (" << (shdr.type == 1 ? "VS" : "PS") << ", " << byte_count << " bytes)... " << std::flush;

    auto shader = std::make_unique<SpirvShader>(shader_type, shdr.ucode_data_hash,
                                                ucode_buffer.data(), dword_count,
                                                std::endian::big);

    disasm_buffer.Reset();
    shader->AnalyzeUcode(disasm_buffer);
    if (!shader->is_ucode_analyzed()) {
      std::cout << "[FALHA ANALISE]" << std::endl;
      continue;
    }

    uint64_t mod_val = 0;
    if (shader_type == xenos::ShaderType::kVertex) {
      mod_val = translator.GetDefaultVertexShaderModification(0);
    } else {
      mod_val = translator.GetDefaultPixelShaderModification(0);
    }

    Shader::Translation* trans = shader->GetOrCreateTranslation(mod_val);
    if (!trans) {
      std::cout << "[FALHA CRIACAO INSTANCIA]" << std::endl;
      continue;
    }

    if (!translator.TranslateAnalyzedShader(*trans) || !trans->is_valid()) {
      std::cout << "[FALHA TRADUCAO]" << std::endl;
      continue;
    }

    const auto& spv = trans->translated_binary();
    if (spv.empty()) {
      std::cout << "[FALHA SPIR-V VAZIO]" << std::endl;
      continue;
    }

    PrebakedShaderEntry entry;
    entry.ucode_hash = shdr.ucode_data_hash;
    entry.shader_type = shader_type;
    entry.texture_bindings = shader->GetTextureBindingsAfterTranslation();
    entry.sampler_bindings = shader->GetSamplerBindingsAfterTranslation();
    entry.spirv_binary = spv;

    cache.AddShader(std::move(entry));
    success++;
    std::cout << "[OK: " << spv.size() << " bytes SPIR-V, " 
              << shader->GetTextureBindingsAfterTranslation().size() << " tex, "
              << shader->GetSamplerBindingsAfterTranslation().size() << " smp]" << std::endl;
  }

  auto t_end = std::chrono::high_resolution_clock::now();
  double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

  std::cout << "\n========================================================" << std::endl;
  std::cout << "Compilacao AOT finalizada em: " << elapsed_ms << " ms" << std::endl;
  std::cout << "Total de shaders compilados:  " << success << " / " << count << std::endl;

  if (cache.SaveToFile(output_cache)) {
    std::cout << "[✓] Cache binario gravado com sucesso: " << output_cache << std::endl;
  } else {
    std::cerr << "[X] Erro ao gravar cache binario." << std::endl;
    return 1;
  }

  // Se solicitado, gerar arquivo C++ header com o bytecode embutido para zero arquivo externo!
  if (!header_output.empty()) {
    std::ofstream h_out(header_output);
    if (h_out) {
      h_out << "// Auto-generated by rex_aot_shader_baker. DO NOT EDIT.\n";
      h_out << "#pragma once\n";
      h_out << "#include <cstdint>\n";
      h_out << "#include <cstddef>\n\n";
      h_out << "namespace rex::graphics::aot {\n\n";

      std::ifstream in(output_cache, std::ios::binary);
      in.seekg(0, std::ios::end);
      size_t f_size = in.tellg();
      in.seekg(0, std::ios::beg);
      std::vector<uint8_t> data(f_size);
      in.read(reinterpret_cast<char*>(data.data()), f_size);

      h_out << "inline constexpr size_t kPrebakedShaderDataSize = " << f_size << ";\n";
      h_out << "inline const uint8_t kPrebakedShaderData[] = {\n";
      for (size_t i = 0; i < f_size; ++i) {
        h_out << "0x" << std::hex << (int)data[i] << ",";
        if ((i + 1) % 16 == 0) h_out << "\n";
      }
      h_out << std::dec << "\n};\n\n";
      h_out << "} // namespace rex::graphics::aot\n";
      std::cout << "[✓] Header C++ estatico gerado: " << header_output << std::endl;
    }
  }

  return 0;
}
