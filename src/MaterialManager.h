#ifndef MATERIALMANAGER_H
#define MATERIALMANAGER_H

#include "Material.h"

#include <assimp/material.h>
#include <assimp/scene.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Expectre {

class MaterialManager {
public:
  static MaterialManager &Instance() {
    static MaterialManager instance;
    return instance;
  }

  // Import material from Assimp, loading associated textures
  MaterialHandle import_material(const aiScene *scene,
                                  const aiMaterial *ai_material,
                                  const std::string &model_directory);

  // Get or create default material (no textures, white color)
  MaterialHandle get_default_material();

  std::vector<MaterialHandle> consume_materials_to_upload_to_gpu() {
    return std::move(m_materials_to_upload_to_gpu);
  }

  uint64_t compute_material_hash(const Material &material) const;

  // Delete copy constructor and assignment operator
  MaterialManager(const MaterialManager &) = delete;
  MaterialManager &operator=(const MaterialManager &) = delete;
  MaterialManager(MaterialManager &&) = delete;
  MaterialManager &operator=(MaterialManager &&) = delete;

  std::optional<std::reference_wrapper<const Material>>
  get_material(MaterialHandle material_handle) {
    auto it = m_material_map.find(material_handle);
    if (it != m_material_map.end()) {
      return std::ref(it->second);
    }
    return std::nullopt;
  }

private:
  MaterialManager() = default;
  ~MaterialManager() = default;

  // Helper to load a texture of a specific type from aiMaterial
  TextureHandle load_texture_from_material(const aiMaterial *ai_material,
                                            aiTextureType texture_type,
                                            const std::string &model_directory);

  std::vector<MaterialHandle> m_materials_to_upload_to_gpu{};
  std::unordered_map<MaterialHandle, Material> m_material_map{};
  MaterialHandle m_default_material_handle{};
};

} // namespace Expectre

#endif // MATERIALMANAGER_H
