#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Material.h"
#include "Texture.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <assimp/material.h>
#include <assimp/scene.h>
#include <spdlog/spdlog.h>
namespace Expectre {
class TextureManager {
public:
  static TextureManager &Instance() {
    static TextureManager instance;
    return instance;
  }

  TextureHandle import_texture(std::string image_directory);
  // TextureHandle TextureManager::import_texture(void *data);
  TextureHandle import_texture(std::string name, void *data, uint32_t width,
                               uint32_t height, uint32_t channels);

  std::vector<TextureHandle> consume_textures_to_upload_to_gpu() {
    return std::move(m_textures_to_upload_to_gpu);
  }

  // Load texture from file - populates CPU data only, no GPU resources
  // Caller is responsible for calling
  // RenderResourceManager::upload_texture_to_gpu
  void load_texture_from_file(Texture &texture, const std::string &filepath);

  uint64_t compute_texture_hash(const Texture &texture) const;

  // Delete copy constructor and assignment operator
  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;
  TextureManager(TextureManager &&) = delete;
  TextureManager &operator=(TextureManager &&) = delete;

  Texture &get_texture(TextureHandle texture) {
    auto it = m_texture_map.find(texture);
    if (it != m_texture_map.end()) {
      return it->second;
    }
    // Texture not found, return default checkerboard
    spdlog::warn(
        "Texture handle {} not found, using default texture",
        texture.texture_id);
    return m_texture_map[m_default_texture_handle];
  }

  TextureHandle get_default_texture();
  Material get_default_material() {
    TextureHandle _default = get_default_texture();
    return Material{"___EXP_DEFAULT_MATERIAL____", _default, _default, _default, _default};
  }

  TextureHandle load_texture_from_material(const aiMaterial *ai_material,
                                           aiTextureType texture_type,
                                           const std::string &model_directory) {

    auto count = ai_material->GetTextureCount(texture_type);
    spdlog::debug("[TEXLOAD] type={} count={} dir='{}'", (int)texture_type,
                  count, model_directory);
    if (count == 0) {
      return get_default_texture(); // Return default
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
      return get_default_texture();
    }

    // Load texture through TextureManager
    auto handle = import_texture(full_path.string());
    spdlog::debug("[TEXLOAD] result texture_id={} valid={}", handle.texture_id,
                  (bool)handle);
    return handle;
  }

  Material import_material(const aiScene *scene, const aiMaterial *ai_material,
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
    spdlog::debug("[MAT] '{}' albedo texture_id={} valid={}", material.name,
                  material.albedo.texture_id,
                  static_cast<bool>(material.albedo));

    material.normal = load_texture_from_material(
        ai_material, aiTextureType_NORMALS, model_directory);

    material.metallic = load_texture_from_material(
        ai_material, aiTextureType_METALNESS, model_directory);

    material.roughness = load_texture_from_material(
        ai_material, aiTextureType_DIFFUSE_ROUGHNESS, model_directory);

    material.ao = load_texture_from_material(
        ai_material, aiTextureType_AMBIENT_OCCLUSION, model_directory);

    return material;
  }

private:
  TextureManager();
  ~TextureManager() = default;

  void create_default_texture();

  std::vector<TextureHandle> m_textures_to_upload_to_gpu{};
  std::unordered_map<TextureHandle, Texture> m_texture_map{};
  TextureHandle m_default_texture_handle{};
};
} // namespace Expectre

#endif // TEXTURE_MANAGER_H