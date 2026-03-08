#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Texture.h"

#include <optional>
#include <unordered_map>
#include <vector>

namespace Expectre {
class TextureManager {
public:
  static TextureManager &Instance() {
    static TextureManager instance;
    return instance;
  }

  TextureHandle import_texture(std::string image_directory);

  std::vector<TextureHandle> consume_textures_to_upload_to_gpu() {
    return std::move(m_textures_to_upload_to_gpu);
  }

  uint64_t compute_texture_hash(const Texture &texture) const;
  // Delete copy constructor and assignment operator
  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;
  TextureManager(TextureManager &&) = delete;
  TextureManager &operator=(TextureManager &&) = delete;

  std::optional<std::reference_wrapper<const Texture>>
  get_texture(TextureHandle texture) {
    auto it = m_texture_map.find(texture);
    if (it != m_texture_map.end()) {
      return std::ref(it->second);
    }
    return std::nullopt;
  }

private:
  TextureManager() = default;
  ~TextureManager() = default;

  uint32_t m_next_mesh_id{0};
  std::vector<TextureHandle> m_textures_to_upload_to_gpu{};
  std::unordered_map<TextureHandle, Texture> m_texture_map{};
};
} // namespace Expectre

#endif // TEXTURE_MANAGER_H