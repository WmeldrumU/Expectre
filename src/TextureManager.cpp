#include "TextureManager.h"
#include <spdlog/spdlog.h>
#include <stb.h>
#define STB_IMAGE_IMPLEMENTATION // includes stb function bodies
#include <stb_image.h>
#include <xxhash.h>

namespace Expectre {

TextureManager::TextureManager() {
  create_default_texture();
}

void TextureManager::create_default_texture() {
  // Create 8x8 magenta checkerboard pattern
  constexpr uint32_t size = 8;
  constexpr uint32_t channels = 4; // RGBA
  uint8_t *data = new uint8_t[size * size * channels];
  
  // Magenta: RGB(255, 0, 255) = 0xFF00FF
  // Black: RGB(0, 0, 0) = 0x000000
  uint8_t magenta[] = {255, 0, 255, 255};
  uint8_t black[] = {0, 0, 0, 255};
  
  for (uint32_t y = 0; y < size; ++y) {
    for (uint32_t x = 0; x < size; ++x) {
      uint32_t pixel_idx = (y * size + x) * channels;
      
      // Checkerboard pattern: alternate based on (x + y) % 2
      bool is_magenta = ((x + y) % 2) == 0;
      uint8_t *color = is_magenta ? magenta : black;
      
      data[pixel_idx + 0] = color[0];     // R
      data[pixel_idx + 1] = color[1];     // G
      data[pixel_idx + 2] = color[2];     // B
      data[pixel_idx + 3] = color[3];     // A
    }
  }
  
  // Import the generated texture
  m_default_texture_handle = import_texture("__default_magenta_checkerboard__", 
                                            data, size, size, channels);
}

uint64_t TextureManager::compute_texture_hash(const Texture &texture) const {
  XXH64_state_t *state = XXH64_createState();
  XXH64_reset(state, 0);

  // Hash pixel data (width * height * channels bytes)
  XXH64_update(state, texture.data,
               static_cast<size_t>(texture.width) * texture.height * texture.channels);

  uint64_t hash = XXH64_digest(state);
  XXH64_freeState(state);

  return hash;
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
  return import_texture(image_dir, data, tex_width, tex_height, desired_channels);
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
  tex.m_name = name;
  tex.data = static_cast<uint8_t *>(data);

  // Compute hash and check for duplicates
  uint64_t hash = compute_texture_hash(tex);
  TextureHandle handle{};
  handle.texture_id = hash;
  if (m_texture_map.find(handle) != m_texture_map.end()) {
    // Texture already exists, return existing ID
    spdlog::error("Texture '{}' (hash - {}) already cached, reusing", tex.m_name,
                  hash);
    return handle;
  }

  m_texture_map[handle] = std::move(tex);
  m_textures_to_upload_to_gpu.push_back(handle);

  return handle;
}

} // namespace Expectre