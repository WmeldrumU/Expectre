#ifndef UI_RENDERER_VK_NOESIS
#define UI_RENDERER_VK_NOESIS

#include <NsRender/RenderDevice.h>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <vector>
namespace Expectre{
class UIRendererVkNoesis : public Noesis::RenderDevice {
public:
  UIRendererVkNoesis() = delete;
  UIRendererVkNoesis(VkPhysicalDevice phys_device, VkDevice device,
                     VmaAllocator allocator);

private:
  // NOESIS RENDERDEVICE INTERFACE

private:
  /// From RenderDevice
  //@{
  const Noesis::DeviceCaps &GetCaps() const override;
  Noesis::Ptr<Noesis::RenderTarget>
  CreateRenderTarget(const char *label, uint32_t width, uint32_t height,
                     uint32_t sampleCount, bool needsStencil) override;
  Noesis::Ptr<Noesis::RenderTarget>
  CloneRenderTarget(const char *label, Noesis::RenderTarget *surface) override;
  Noesis::Ptr<Noesis::Texture> CreateTexture(const char *label, uint32_t width,
                                             uint32_t height,
                                             uint32_t numLevels,
                                             Noesis::TextureFormat::Enum format,
                                             const void **data) override;
  void UpdateTexture(Noesis::Texture *texture, uint32_t level, uint32_t x,
                     uint32_t y, uint32_t width, uint32_t height,
                     const void *data) override;
  void BeginOffscreenRender() override;
  void EndOffscreenRender() override;
  void BeginOnscreenRender() override;
  void EndOnscreenRender() override;
  void SetRenderTarget(Noesis::RenderTarget *surface) override;
  void BeginTile(Noesis::RenderTarget *surface,
                 const Noesis::Tile &tile) override;
  void EndTile(Noesis::RenderTarget *surface) override;
  void ResolveRenderTarget(Noesis::RenderTarget *surface,
                           const Noesis::Tile *tiles,
                           uint32_t numTiles) override;
  void *MapVertices(uint32_t bytes) override;
  void UnmapVertices() override;
  void *MapIndices(uint32_t bytes) override;
  void UnmapIndices() override;
  void DrawBatch(const Noesis::Batch &batch) override;
  //@}

  VkPhysicalDevice m_phys_device;
  VkDevice m_device;
  VmaAllocator m_allocator;
  std::vector<AllocatedBuffer> m_allocations;
  
};
} // namespace Expectre
#endif // UI_RENDERER_VK_NOESIS