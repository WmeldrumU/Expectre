#include "TextureManager.h"
#include <spdlog/spdlog.h>
#include <stb.h>
#define STB_IMAGE_IMPLEMENTATION // includes stb function bodies
#include <stb_image.h>
#include <xxhash.h>

namespace Expectre {

uint64_t TextureManager::compute_texture_hash(const Texture &texture) const {
  XXH64_state_t *state = XXH64_createState();
  XXH64_reset(state, 0);

  // Hash vertex data
  XXH64_update(state, texture.data,
               sizeof(Texture::data) * texture.width * texture.height);

  uint64_t hash = XXH64_digest(state);
  XXH64_freeState(state);

  return hash;
  return 0;
}

TextureHandle TextureManager::import_texture(std::string image_dir) {
  int tex_width, tex_height, tex_channels;
  constexpr int desired_channels = 4; // Force RGBA
  stbi_uc *data = stbi_load(image_dir.c_str(), &tex_width, &tex_height,
                            &tex_channels, desired_channels); // Force RGBA

  if (!data) {
    spdlog::error("Failed to load texture from '{}'", image_dir);
    std::terminate();
  }
  import_texture(image_dir, data, tex_width, tex_height, desired_channels);
}

TextureHandle TextureManager::import_texture(std::string name, void *data,
                                             uint32_t width, uint32_t height,
                                             uint32_t channels) {
  if (!data) {
    spdlog::error("passed image data is null");
    return {};
  }

  Texture tex{};
  tex.channels = channels;
  tex.width = width;
  tex.height = height;
  tex.name = name;
  tex.data = static_cast<uint8_t *>(data);

  // Compute hash and check for duplicates
  uint64_t hash = compute_texture_hash(tex);
  TextureHandle handle{};
  handle.texture_id = hash;
  if (m_texture_map.find(handle) != m_texture_map.end()) {
    // Texture already exists, return existing ID
    spdlog::error("Texture '{}' (hash - {}) already cached, reusing", tex.name,
                  hash);
    return handle;
  }

  m_texture_map[handle] = std::move(tex);
  m_textures_to_upload_to_gpu.push_back(handle);

  return handle;
}

} // namespace Expectre