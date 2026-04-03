#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Texture.h"

#include <spdlog/spdlog.h>
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
  //TextureHandle TextureManager::import_texture(void *data);
  TextureHandle import_texture(std::string name, void *data,
                                             uint32_t width, uint32_t height,
                                             uint32_t channels);

  std::vector<TextureHandle> consume_textures_to_upload_to_gpu() {
    return std::move(m_textures_to_upload_to_gpu);
  }

  uint64_t compute_texture_hash(const Texture &texture) const;
  
  // Delete copy constructor and assignment operator
  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;
  TextureManager(TextureManager &&) = delete;
  TextureManager &operator=(TextureManager &&) = delete;

  const Texture &get_texture(TextureHandle texture) {
    auto it = m_texture_map.find(texture);
    if (it != m_texture_map.end()) {
      return it->second;
    }
    // Texture not found, return default magenta checkerboard
    spdlog::warn("Texture handle {} not found, using default magenta checkerboard", 
                 texture.texture_id);
    return m_texture_map[m_default_texture_handle];
  }

private:
  TextureManager();
  ~TextureManager() = default;

  void create_default_texture();

  uint32_t m_next_tex_id{0};
  std::vector<TextureHandle> m_textures_to_upload_to_gpu{};
  std::unordered_map<TextureHandle, Texture> m_texture_map{};
  TextureHandle m_default_texture_handle{};
};
} // namespace Expectre

#endif // TEXTURE_MANAGER_H