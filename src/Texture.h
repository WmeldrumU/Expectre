#ifndef TEXTURE_H
#define TEXTURE_H
#include <stb_image.h>
#include <string>
#include <vulkan/vulkan.h>

// #include "Resource.h"

namespace Expectre {

struct Texture /*: public Resource*/ {
  // CPU data
  uint8_t *data = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t channels = 0;
  std::string name;

  ~Texture() {
    if (data != nullptr)
      stbi_image_free(data);
  }

  // Move Constructor
  Texture(Texture &&other) noexcept
      : /* Resource(std::move(other)),*/
        data(std::exchange(other.data, nullptr)), width(other.width),
        height(other.height), channels(other.channels),
        name(std::move(other.name)) {}

  // Move Assignment Operator (i.e. t2 = std::move(t1); )
  Texture &operator=(Texture &&other) noexcept {
    if (this != &other) { // prevent self assignment
      if (data) {
        stbi_image_free(data); // clean up self image before taking other's
      }
      data = std::exchange(other.data, nullptr); // take others image
      width = other.width;
      height = other.height;
      channels = other.channels;
      name = std::move(other.name);
    }
    return *this;
  }

  // Disable Copying (Rule of Five) to prevent double-frees
  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;
};

} // namespace Expectre

#endif // TEXTURE_H