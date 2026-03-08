#include "UIRendererVkNoesis.h"
#include "Texture.h"
#include "TextureManager.h"
#include "TextureVk.h"
namespace Expectre {
const Noesis::DeviceCaps &UiRendererVkNoesis::GetCaps() const {
  static Noesis::DeviceCaps caps = []() {
    Noesis::DeviceCaps c{};
    c.centerPixelOffset = 0.0f;
    c.clipSpaceYInverted = false;
    c.depthRangeZeroToOne = true;
    c.linearRendering = true;
    c.subpixelRendering = false;
    return c;
  }();
  return caps;
}

Noesis::Ptr<Noesis::Texture> UiRendererVkNoesis::CreateTexture(
    const char *label, uint32_t width, uint32_t height, uint32_t numLevels,
    Noesis::TextureFormat::Enum format, const void **data) {

  Texture tex;
  tex.width = width;
  tex.height = height;
  tex.channels = 4;
  tex.name = std::string(label);

  // clang-format off
  /*
  Noesis texture formats
  RGBA8 -  A four-component format that supports 8 bits per channel, including alpha
  RGBX8 - A four-component format that supports 8 bits for each color, channel and 8 bits unused
  R8 - A single-component format that supports 8 bits for the red channel

  
  Count - number of different formats
  */
  // clang-format on

  // Set channels to 1 if r8 format, otherwise always use 4 channels
  if (format == Noesis::TextureFormat::Enum::R8) {
    tex.channels = 1;
  }

  return nullptr;
}

} // namespace Expectre