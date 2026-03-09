#include "MaterialManager.h"
#include "TextureManager.h"
#include <filesystem>
#include <spdlog/spdlog.h>
#include <xxhash.h>

namespace Expectre {

uint64_t
MaterialManager::compute_material_hash(const Material &material) const {
  XXH64_state_t *state = XXH64_createState();
  XXH64_reset(state, 0);

  // Hash material name
  XXH64_update(state, material.name.data(), material.name.size());

  // Hash texture handles
  XXH64_update(state, &material.albedo, sizeof(TextureHandle));
  XXH64_update(state, &material.normal, sizeof(TextureHandle));
  XXH64_update(state, &material.metallic, sizeof(TextureHandle));
  XXH64_update(state, &material.roughness, sizeof(TextureHandle));
  XXH64_update(state, &material.ao, sizeof(TextureHandle));

  uint64_t hash = XXH64_digest(state);
  XXH64_freeState(state);

  return hash;
}

TextureHandle MaterialManager::load_texture_from_material(
    const aiMaterial *ai_material, aiTextureType texture_type,
    const std::string &model_directory) {

  auto count = ai_material->GetTextureCount(texture_type);
  spdlog::debug("[TEXLOAD] type={} count={} dir='{}'", (int)texture_type, count, model_directory);
  if (count == 0) {
    return TextureHandle{}; // Return invalid handle if no texture
  }

  aiString texture_path;
  ai_material->GetTexture(texture_type, 0, &texture_path);

  // Construct full path
  std::filesystem::path full_path =
      std::filesystem::path(model_directory) / texture_path.C_Str();

  bool exists = std::filesystem::exists(full_path);
  spdlog::debug("[TEXLOAD] path='{}' exists={}", full_path.string(), exists);

  if (!exists) {
    spdlog::warn("Texture file not found: {}", full_path.string());
    return TextureHandle{};
  }

  // Load texture through TextureManager
  auto handle = TextureManager::Instance().import_texture(full_path.string());
  spdlog::debug("[TEXLOAD] result texture_id={} valid={}", handle.texture_id, (bool)handle);
  return handle;
}

MaterialHandle
MaterialManager::import_material(const aiScene *scene,
                                  const aiMaterial *ai_material,
                                  const std::string &model_directory) {
  Material material{};

  // Get material name
  aiString ai_name;
  if (ai_material->Get(AI_MATKEY_NAME, ai_name) == AI_SUCCESS) {
    material.name = ai_name.C_Str();
  }

  // Load PBR textures
  material.albedo = load_texture_from_material(
      ai_material, aiTextureType_DIFFUSE, model_directory);
  spdlog::debug("[MAT] '{}' albedo texture_id={} valid={}",
               material.name, material.albedo.texture_id, (bool)material.albedo);

  material.normal = load_texture_from_material(
      ai_material, aiTextureType_NORMALS, model_directory);

  material.metallic = load_texture_from_material(
      ai_material, aiTextureType_METALNESS, model_directory);

  material.roughness = load_texture_from_material(
      ai_material, aiTextureType_DIFFUSE_ROUGHNESS, model_directory);

  material.ao = load_texture_from_material(
      ai_material, aiTextureType_AMBIENT_OCCLUSION, model_directory);

  // Compute hash and check for duplicates
  uint64_t hash = compute_material_hash(material);
  MaterialHandle handle{};
  handle.material_id = hash;

  if (m_material_map.find(handle) != m_material_map.end()) {
    // Material already exists, return existing handle
    spdlog::debug("Material '{}' already cached, reusing", material.name);
    return handle;
  }

  // New unique material
  m_material_map[handle] = std::move(material);
  m_materials_to_upload_to_gpu.push_back(handle);

  spdlog::info("Imported material: {}", m_material_map[handle].name);

  return handle;
}

MaterialHandle MaterialManager::get_default_material() {
  // Return cached default material if already created
  if (m_default_material_handle) {
    return m_default_material_handle;
  }

  // Create default material with no textures
  Material default_material{};
  default_material.name = "DefaultMaterial";
  // All texture handles are invalid by default

  // Compute hash and store
  uint64_t hash = compute_material_hash(default_material);
  m_default_material_handle.material_id = hash;

  // Add to map
  m_material_map[m_default_material_handle] = std::move(default_material);
  m_materials_to_upload_to_gpu.push_back(m_default_material_handle);

  spdlog::info("Created default material");

  return m_default_material_handle;
}

} // namespace Expectre
