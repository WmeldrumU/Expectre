#ifndef TEXTURE_H
#define TEXTURE_H
#include <stb_image.h>
#include <string>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

// #include "Resource.h"

namespace Expectre {
struct TextureHandle {
  uint64_t texture_id = -1;
  explicit operator bool() const { return texture_id != -1; }

  // Equality operator for use in unordered_map
  bool operator==(const TextureHandle &other) const {
    return texture_id == other.texture_id;
  }
};

class Texture /*: public Resource*/ {
public:
  enum class Source { File, Internal };

  Texture()
      : /*Resource("__internal_textuire__"), */ m_source(Source::Internal) {}
  Texture(const std::string &path)
      : /*Resource(path), */ m_source(Source::File) {}
  ~Texture() {
    if (data != nullptr)
      stbi_image_free(data);
  }

  // Move Constructor
  Texture(Texture &&other) noexcept
      : /* Resource(std::move(other)),*/
        data(std::exchange(other.data, nullptr)), width(other.width),
        height(other.height), channels(other.channels),
        m_name(std::move(other.m_name)) {}

  // Move Assignment Operator (i.e. t2 = std::move(t1); )
  Texture &operator=(Texture &&other) noexcept {
    if (this != &other) { // Prevent self-assignment
      if (data) {
        stbi_image_free(data); // Clean up existing resource first
      }
      data = std::exchange(other.data, nullptr); // Steal the new one
    }
    return *this;
  }

  // Disable Copying (Rule of Five) to prevent double-frees
  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;

  bool is_internal() const { return m_source == Source::Internal; }

  // Resource interface
  // bool load() override {

  //   if (!is_internal()) {
  //     // No CPU image to load for internal textures
  //     return false;

  //     int w, h, ch;
  //     data = stbi_load(m_name.c_str(), &w, &h, &ch, STBI_rgb_alpha);
  //     if (!data)
  //       return false;

  //     width = static_cast<uint32_t>(w);
  //     height = static_cast<uint32_t>(h);
  //     channels = 4; // forced RGBA
  //   }

  //   // TODO: create VkImage, VkImageView, VkSampler
  //   // Use ResourceManager::gpu() context for Vulkan objects
  //   m_loaded = true;
  //   return true;
  // }

  // bool unload() override {
  //   // TODO: destroy VkImage, VkImageView, VkSampler
  //   m_loaded = false;

  //   return true;
  // }

  // CPU data
  uint8_t *data = nullptr;
  uint32_t width = -1;
  uint32_t height = -1;
  uint8_t channels = -1;
  std::string m_name;
  // GPU data (filled after upload)
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;
  VmaAllocation allocation;

private:
  // Indicates whether the texture data is loaded from a file or
  // if it is used for internal purposes (i.e. depth stencil)
  Source m_source = Source::File;
};

} // namespace Expectre

// Hashing function for Texture for use as a key in unordered_map

namespace std {
template <> struct hash<Expectre::TextureHandle> {
  std::size_t operator()(const Expectre::TextureHandle &handle) const {
    return std::hash<uint64_t>{}(handle.texture_id);
  }
};
} // namespace std
#endif // TEXTURE_H