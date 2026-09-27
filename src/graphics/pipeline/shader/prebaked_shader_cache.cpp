#include <rex/graphics/pipeline/shader/prebaked_shader_cache.h>

#include <cstring>
#include <fstream>
#include <iostream>

#include <rex/logging.h>

namespace rex::graphics {

PrebakedShaderCache& PrebakedShaderCache::Get() {
  static PrebakedShaderCache instance;
  return instance;
}

void PrebakedShaderCache::Clear() {
  entries_.clear();
}

void PrebakedShaderCache::AddShader(PrebakedShaderEntry entry) {
  entries_[entry.ucode_hash] = std::move(entry);
}

const PrebakedShaderEntry* PrebakedShaderCache::FindShader(uint64_t ucode_hash) const {
  auto it = entries_.find(ucode_hash);
  if (it != entries_.end()) {
    return &it->second;
  }
  return nullptr;
}

bool PrebakedShaderCache::HasShader(uint64_t ucode_hash) const {
  return entries_.find(ucode_hash) != entries_.end();
}

bool PrebakedShaderCache::SaveToFile(const std::string& file_path) const {
  std::ofstream out(file_path, std::ios::binary);
  if (!out) {
    REXLOG_ERROR("PrebakedShaderCache: Nao foi possivel abrir {} para gravacao", file_path);
    return false;
  }

  uint32_t magic = kMagic;
  uint32_t version = kVersion;
  uint32_t title_id = kDefaultTitleId;
  uint32_t entry_count = static_cast<uint32_t>(entries_.size());

  out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
  out.write(reinterpret_cast<const char*>(&version), sizeof(version));
  out.write(reinterpret_cast<const char*>(&title_id), sizeof(title_id));
  out.write(reinterpret_cast<const char*>(&entry_count), sizeof(entry_count));

  for (const auto& [hash, entry] : entries_) {
    out.write(reinterpret_cast<const char*>(&entry.ucode_hash), sizeof(entry.ucode_hash));
    uint32_t type_val = static_cast<uint32_t>(entry.shader_type);
    out.write(reinterpret_cast<const char*>(&type_val), sizeof(type_val));

    uint32_t tb_count = static_cast<uint32_t>(entry.texture_bindings.size());
    out.write(reinterpret_cast<const char*>(&tb_count), sizeof(tb_count));
    if (tb_count > 0) {
      out.write(reinterpret_cast<const char*>(entry.texture_bindings.data()),
                tb_count * sizeof(SpirvShader::TextureBinding));
    }

    uint32_t sb_count = static_cast<uint32_t>(entry.sampler_bindings.size());
    out.write(reinterpret_cast<const char*>(&sb_count), sizeof(sb_count));
    if (sb_count > 0) {
      out.write(reinterpret_cast<const char*>(entry.sampler_bindings.data()),
                sb_count * sizeof(SpirvShader::SamplerBinding));
    }

    uint32_t spv_size = static_cast<uint32_t>(entry.spirv_binary.size());
    out.write(reinterpret_cast<const char*>(&spv_size), sizeof(spv_size));
    if (spv_size > 0) {
      out.write(reinterpret_cast<const char*>(entry.spirv_binary.data()), spv_size);
    }
  }

  REXLOG_INFO("PrebakedShaderCache: Salvo com sucesso {} shaders em {}", entry_count, file_path);
  return true;
}

bool PrebakedShaderCache::LoadFromFile(const std::string& file_path) {
  std::ifstream in(file_path, std::ios::binary);
  if (!in) {
    return false;
  }

  in.seekg(0, std::ios::end);
  size_t size = in.tellg();
  in.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(size);
  if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) {
    return false;
  }

  return LoadFromMemory(buffer.data(), size);
}

bool PrebakedShaderCache::LoadFromMemory(const void* data, size_t size) {
  if (!data || size < 16) {
    return false;
  }

  const uint8_t* ptr = static_cast<const uint8_t*>(data);
  const uint8_t* end = ptr + size;

  uint32_t magic = 0;
  uint32_t version = 0;
  uint32_t title_id = 0;
  uint32_t entry_count = 0;

  std::memcpy(&magic, ptr, sizeof(magic)); ptr += sizeof(magic);
  std::memcpy(&version, ptr, sizeof(version)); ptr += sizeof(version);
  std::memcpy(&title_id, ptr, sizeof(title_id)); ptr += sizeof(title_id);
  std::memcpy(&entry_count, ptr, sizeof(entry_count)); ptr += sizeof(entry_count);

  if (magic != kMagic || version != kVersion) {
    REXLOG_ERROR("PrebakedShaderCache: Cabecalho invalido ou versao incompativel");
    return false;
  }

  entries_.reserve(entries_.size() + entry_count);

  for (uint32_t i = 0; i < entry_count; ++i) {
    if (ptr + sizeof(uint64_t) + sizeof(uint32_t) * 4 > end) {
      REXLOG_ERROR("PrebakedShaderCache: Buffer corrompido ou truncado");
      return false;
    }

    PrebakedShaderEntry entry;
    std::memcpy(&entry.ucode_hash, ptr, sizeof(entry.ucode_hash)); ptr += sizeof(entry.ucode_hash);
    
    uint32_t type_val = 0;
    std::memcpy(&type_val, ptr, sizeof(type_val)); ptr += sizeof(type_val);
    entry.shader_type = static_cast<xenos::ShaderType>(type_val);

    uint32_t tb_count = 0;
    std::memcpy(&tb_count, ptr, sizeof(tb_count)); ptr += sizeof(tb_count);
    if (tb_count > 0) {
      size_t tb_bytes = tb_count * sizeof(SpirvShader::TextureBinding);
      if (ptr + tb_bytes > end) return false;
      entry.texture_bindings.resize(tb_count);
      std::memcpy(entry.texture_bindings.data(), ptr, tb_bytes);
      ptr += tb_bytes;
    }

    uint32_t sb_count = 0;
    std::memcpy(&sb_count, ptr, sizeof(sb_count)); ptr += sizeof(sb_count);
    if (sb_count > 0) {
      size_t sb_bytes = sb_count * sizeof(SpirvShader::SamplerBinding);
      if (ptr + sb_bytes > end) return false;
      entry.sampler_bindings.resize(sb_count);
      std::memcpy(entry.sampler_bindings.data(), ptr, sb_bytes);
      ptr += sb_bytes;
    }

    uint32_t spv_size = 0;
    std::memcpy(&spv_size, ptr, sizeof(spv_size)); ptr += sizeof(spv_size);
    if (spv_size > 0) {
      if (ptr + spv_size > end) return false;
      entry.spirv_binary.resize(spv_size);
      std::memcpy(entry.spirv_binary.data(), ptr, spv_size);
      ptr += spv_size;
    }

    entries_[entry.ucode_hash] = std::move(entry);
  }

  REXLOG_INFO("PrebakedShaderCache: Carregados {} shaders pre-compilados na memoria", entry_count);
  return true;
}

}  // namespace rex::graphics
