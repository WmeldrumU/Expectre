#ifndef TEXTURE_H
#define TEXTURE_H
#include <stb_image.h>
#include <string>
namespace Expectre {
struct TextureHandle {
  uint64_t texture_id = -1;
  explicit operator bool() const { return texture_id != -1; }

  // Equality operator for use in unordered_map
  bool operator==(const TextureHandle &other) const {
    return texture_id == other.texture_id;
  }
};

struct Texture {

  Texture() {}
  ~Texture() {
    if (data != nullptr)
      stbi_image_free(data);
  }

  // Move Constructor (i.e. Texture t2 = std::move(t1); )
  // Steals 'other.data' and sets 'other.data' to nullptr
  Texture(Texture &&other) noexcept
      : data(std::exchange(other.data, nullptr)) {}

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

  uint8_t *data = nullptr;
  uint32_t width = -1;
  uint32_t height = -1;
  uint8_t channels = -1;
  std::string name;
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