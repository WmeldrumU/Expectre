#include "UIRendererVkNoesis.h"
#include "ToolsVk.h"
namespace Expectre {
UIRendererVkNoesis::UIRendererVkNoesis(VkPhysicalDevice phys_device,
                                       VkDevice device, VmaAllocator allocator)
    : m_phys_device{phys_device}, m_device{device}, m_allocator{allocator} {

  constexpr VkDeviceSize i_buf_size = static_cast<VkDeviceSize>(1024 * 1024 * 8);
  constexpr VkDeviceSize v_buf_size = static_cast<VkDeviceSize>(1024 * 1024 * 8);

  for (int i = 0; i < 2; ++i) {
    m_frames[i].ib = ToolsVk::create_buffer(
        m_allocator, i_buf_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "NOESIS IB " + std::to_string(i));

    VK_CHECK_RESULT(vmaMapMemory(m_allocator, m_frames[i].ib.allocation,
                                 (void **)&m_frames[i].ibMapped));

    m_frames[i].vb = ToolsVk::create_buffer(
        m_allocator, v_buf_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "NOESIS VB " + std::to_string(i));

    VK_CHECK_RESULT(vmaMapMemory(m_allocator, m_frames[i].vb.allocation,
                                 (void **)&m_frames[i].vbMapped));
  }
}
const Noesis::DeviceCaps &UIRendererVkNoesis::GetCaps() const {
  static Noesis::DeviceCaps caps;
  caps.linearRendering = true;
  caps.subpixelRendering = false;
  caps.depthRangeZeroToOne = true;
  caps.clipSpaceYInverted = false;
  return caps;
}

UIRendererVkNoesis::~UIRendererVkNoesis() {
  for (int i = 0; i < 2; ++i) {
    if (m_frames[i].vbMapped)
      vmaUnmapMemory(m_allocator, m_frames[i].vb.allocation);
    if (m_frames[i].ibMapped)
      vmaUnmapMemory(m_allocator, m_frames[i].ib.allocation);

    vmaDestroyBuffer(m_allocator, m_frames[i].vb.buffer,
                     m_frames[i].vb.allocation);
    vmaDestroyBuffer(m_allocator, m_frames[i].ib.buffer,
                     m_frames[i].ib.allocation);
  }
}

void UIRendererVkNoesis::BeginRender(uint32_t frame_index) {
  m_frameIndex = frame_index;
  auto &f = m_frames[m_frameIndex];
  f.vbHead = 0;
  f.ibHead = 0;
}

void UIRendererVkNoesis::BeginTile(Noesis::RenderTarget *surface,
                                   const Noesis::Tile &tile) {}
void UIRendererVkNoesis::EndTile(Noesis::RenderTarget *surface) {}

void *UIRendererVkNoesis::MapVertices(uint32_t bytes) {
  auto &f = m_frames[m_frameIndex];
  // align head for safety (16 is fine)
  f.vbHead = (f.vbHead + 15u) & ~15u;

  f.lastVBBase = f.vbHead; // base of “last pointer returned”
  void *ptr = f.vbMapped + f.vbHead;

  f.vbHead += bytes; // (optionally align up after)
  return ptr;
}
void UIRendererVkNoesis::UnmapVertices() {}
void *UIRendererVkNoesis::MapIndices(uint32_t bytes) {

  auto &f = m_frames[m_frameIndex];
  f.ibHead = (f.ibHead + 3u) & ~3u;

  f.lastIBBase = f.ibHead;
  void *ptr = f.ibMapped + f.ibHead;

  f.ibHead += bytes;
  return ptr;
}
void UIRendererVkNoesis::UnmapIndices() {}

void UIRendererVkNoesis::BeginOnscreenRender() {}

} // namespace Expectre