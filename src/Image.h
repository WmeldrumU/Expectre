#ifndef IMAGE_H
#define IMAGE_H
#include <array>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string>

#include <stb_image.h>

namespace Expectre {

struct Image {
  // CPU data
  uint8_t *data = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t channels = 0;
  std::string name;
  Image() = default;
  ~Image() {
    if (data != nullptr)
      stbi_image_free(data);
  }

  // Move Constructor
  Image(Image &&other) noexcept
      : /* Resource(std::move(other)),*/
        data(std::exchange(other.data, nullptr)), width(other.width),
        height(other.height), channels(other.channels),
        name(std::move(other.name)) {}

  // Move Assignment Operator (i.e. t2 = std::move(t1); )
  Image &operator=(Image &&other) noexcept {
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
  Image(const Image &) = delete;
  Image &operator=(const Image &) = delete;
};

inline const Image &get_default_image() {
  // 1. Thread-safe initialization guard guarantees this entire block runs
  // EXACTLY ONCE
  static Image default_image;

  if (default_image.data == nullptr) {
    constexpr uint32_t image_size = 64;
    constexpr uint32_t block_size = 8;
    constexpr uint32_t channels = 4;

    // 2. Allocate the entire 64x64 grid statically on the data segment.
    // Footprint is exactly 16 KB (64 * 64 * 4 bytes). It lives forever and
    // never leaks.
    static std::array<uint8_t, image_size * image_size * channels>
        checker_pixels;

    const uint8_t magenta[] = {255, 0, 255, 255};
    const uint8_t black[] = {0, 0, 0, 255};

    // 3. Compute checkerboard patterns onto the static memory buffer layout
    for (uint32_t y = 0; y < image_size; ++y) {
      for (uint32_t x = 0; x < image_size; ++x) {
        // Determines if the grid coordinates map to an odd or even square cell
        const bool use_magenta =
            (((x / block_size) + (y / block_size)) % 2) == 0;
        const uint8_t *color = use_magenta ? magenta : black;

        uint32_t pixel_index = (y * image_size + x) * channels;

        checker_pixels[pixel_index + 0] = color[0];
        checker_pixels[pixel_index + 1] = color[1];
        checker_pixels[pixel_index + 2] = color[2];
        checker_pixels[pixel_index + 3] = color[3];
      }
    }

    // 4. Fill the persistent tracking fields cleanly
    default_image.data = checker_pixels.data();
    default_image.width = image_size;
    default_image.height = image_size;
    default_image.channels = channels;
    default_image.name = "__default_checkerboard__";
  }

  return default_image;
}

inline Image import_from_file(const std::filesystem::path &path) {
  int width = 0;
  int height = 0;
  int original_channels = 0;

  constexpr int desired_channels = 4;

  stbi_uc *pixels = stbi_load(path.string().c_str(), &width, &height,
                              &original_channels, desired_channels);

  if (!pixels) {
    spdlog::error("Failed to load texture '{}': {}", path.string(),
                  stbi_failure_reason());
    return Image();
  }

  Image out_image;
  out_image.data = pixels;
  out_image.width = static_cast<uint32_t>(width);
  out_image.height = static_cast<uint32_t>(height);
  out_image.channels = static_cast<uint8_t>(desired_channels);
  out_image.name = path.generic_string();
  return out_image;
}

inline Image import_from_memory(std::string name, const uint8_t *bytes,
                                size_t byte_count) {

  if (!bytes || byte_count == 0) {
    spdlog::error("Cannot load texture '{}': image bytes are empty", name);
    return Image();
  }

  int width = 0;
  int height = 0;
  int original_channels = 0;
  constexpr int desired_channels = 4;
  stbi_uc *pixels =
      stbi_load_from_memory(bytes, static_cast<int>(byte_count), &width,
                            &height, &original_channels, desired_channels);

  if (!pixels) {
    spdlog::error("Failed to decode texture '{}': {}", name,
                  stbi_failure_reason());
    return Image();
  }

  Image out_image;
  out_image.data = pixels;
  out_image.width = static_cast<uint32_t>(width);
  out_image.height = static_cast<uint32_t>(height);
  out_image.channels = static_cast<uint8_t>(desired_channels);
  out_image.name = std::move(name);
  return out_image;
}

} // namespace Expectre

#endif // IMAGE_H