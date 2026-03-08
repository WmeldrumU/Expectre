#ifndef RENDER_TARGET_VK_NS
#define RENDER_TARGET_VK_NS

#include <NsRender/Texture.h>
#include <NsRender/RenderTarget.h>

#include "noesis/TextureVkNs.h"

namespace Expectre {
// Wrapper class, wrapping expectre's TextureVk to play nice with noesis
////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderTargetVkNs final : public Noesis::RenderTarget {
public:
  ~RenderTargetVkNs() { color->device->SafeReleaseRenderTarget(this); }

  // Noesis::Rendertarget Interface
  Noesis::Texture *GetTexture() override { return color.GetPtr(); }

  // end Noesis::Rendertarget Interface

  Noesis::Ptr<TextureVkNs> color;
 // Noesis::Ptr<TextureVkNs> colorAA;
  Noesis::Ptr<TextureVkNs> stencil;

  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
};
} // namespace Expectre
#endif // RENDER_TARGET_VK_NS