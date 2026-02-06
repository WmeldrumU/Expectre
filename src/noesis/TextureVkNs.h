#ifndef TEXTURE_VK_NS
#define TEXTURE_VK_NS

#include <NsRender/Texture.h>

#include "TextureVk.h"
namespace Expectre {

// Wrapper class, wrapping Expectre's TextureVk to play nice with Noesis

class TextureVkNs final : public Noesis::Texture {
public:
  TextureVkNs(uint32_t hash) /*: hash(hash)*/ {}
  ~TextureVkNs() {}

  // Noesis::Texture interface - delegates to expectre's TextureVk class
  uint32_t GetWidth() const override {
    return exp_texture.image_info.extent.width;
  }
  uint32_t GetHeight() const override {
    return exp_texture.image_info.extent.height;
  }
  bool HasMipMaps() const override {
    return exp_texture.image_info.mipLevels > 1;
  }
  bool IsInverted() const override { return false; }
  bool HasAlpha() const override { return true; }

  TextureVk exp_texture;
};
} // namespace Expectre
#endif // TEXTURE_VK_NS