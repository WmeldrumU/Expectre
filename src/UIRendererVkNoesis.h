// #ifndef UI_RENDERER_VK_NOESIS
// #define UI_RENDERER_VK_NOESIS

// // #include <NsApp/ThemeProviders.h>
// // #include <NsRender/GLFactory.h>
// #include <NsGui/IRenderer.h>
// #include <NsGui/IntegrationAPI.h>
// #include <NsRender/RenderDevice.h>
// // #include <NsGui/IView.h>
// // #include <NsGui/Grid.h>

// // #include "IUIRenderer.h"
// #include "TextureVk.h"
// namespace Expectre {

// class UiRendererVkNoesis : Noesis::RenderDevice {

//   const DeviceCaps &UiRendererVkNoesis::GetCaps() const {
//     static DeviceCaps caps = []() {
//       Noesis::DeviceCaps c{};
//       c.centerPixelOffset = 0.0f;
//       c.clipSpaceYInverted = false;
//       c.depthRangeZeroToOne = true;
//       c.linearRendering = true;
//       c.subpixelRendering = false;
//       return c;
//     }();
//     return caps;
//   }

//   Noesis::Ptr<Noesis::RenderTarget>
//   CreateRenderTarget(const char *label, uint32_t width, uint32_t height,
//                      uint32_t sampleCount, bool needsStencil) override {
//     Ptr<RenderTargetVkNs> target = MakePtr<RenderTargetVkNs>();
//     target->samples = VK_SAMPLE_COUNT_1_BIT;
//     target->

//         Vector<VkImageView, 3>
//             attachments;

//     if (needsStencil) {
//       target->stencil = TextureVk::create_depth_stencil(
//           m_physical_device, device, m_cmd_pool, m_graphics_queue, allocator,
//           m_extent);
//       attachments.PushBack(surface->stencil->view);

//       TextureVk::transition_image_layout(
//           device, cmd_pool, graphics_queue, target->stencil->image,
//           target->stencil->image_info.format, target->stencil->layout,
//           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
//     }
//   }
//   Noesis::Ptr<Noesis::RenderTarget>
//   CloneRenderTarget(const char *label, Noesis::RenderTarget *surface) override {

//   }

//   Noesis::Ptr<Noesis::Texture> CreateTexture(const char *label, uint32_t width,
//                                              uint32_t height,
//                                              uint32_t numLevels,
//                                              Noesis::TextureFormat::Enum format,
//                                              const void **data) override;
//   void UpdateTexture(Noesis::Texture *texture, uint32_t level, uint32_t x,
//                      uint32_t y, uint32_t width, uint32_t height,
//                      const void *data) override;
//   void BeginOffscreenRender() override;
//   void EndOffscreenRender() override;
//   void BeginOnscreenRender() override;
//   void EndOnscreenRender() override;
//   void SetRenderTarget(Noesis::RenderTarget *surface) override;
//   void BeginTile(Noesis::RenderTarget *surface,
//                  const Noesis::Tile &tile) override;
//   void EndTile(Noesis::RenderTarget *surface) override;
//   void ResolveRenderTarget(Noesis::RenderTarget *surface,
//                            const Noesis::Tile *tiles,
//                            uint32_t numTiles) override;
//   void *MapVertices(uint32_t bytes) override;
//   void UnmapVertices() override;
//   void *MapIndices(uint32_t bytes) override;
//   void UnmapIndices() override;
//   void DrawBatch(const Noesis::Batch &batch) override;
//   //@}
// }
// } // namespace Expectre
// #endif UI_RENDERER_VK_NOESIS