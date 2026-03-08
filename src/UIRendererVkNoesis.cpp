#include "UIRendererVkNoesis.h"
#include "ToolsVk.h"
namespace Expectre {
UIRendererVkNoesis::UIRendererVkNoesis(VkPhysicalDevice phys_device,
                                       VkDevice device, VmaAllocator allocator)
    : m_phys_device{phys_device}, m_device{device}, m_allocator{allocator} {}
const Noesis::DeviceCaps &UIRendererVkNoesis::GetCaps() const {
  static Noesis::DeviceCaps caps;
  caps.linearRendering = true;
  caps.subpixelRendering = false;
  caps.depthRangeZeroToOne = true;
  caps.clipSpaceYInverted = false;
  return caps;
}

UIRendererVkNoesis::~UIRendererVkNoesis() {
  for (const auto &alloc : m_allocations) {
    vmaDestroyBuffer(m_allocator, alloc.buffer, alloc.allocation);
  }
}

void UIRendererVkNoesis::BeginTile(Noesis::RenderTarget *surface,
                                   const Noesis::Tile &tile) {}
void UIRendererVkNoesis::EndTile(Noesis::RenderTarget *surface) {}

void *UIRendererVkNoesis::MapVertices(uint32_t bytes) {

  VkDeviceSize vk_bytes = static_cast<VkDeviceSize>(bytes);
  static uint32_t ns_vert_mapping = 1;
  AllocatedBuffer buffer = ToolsVk::create_buffer(
      m_allocator, vk_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      "NOESIS" + std::to_string(ns_vert_mapping));

  void *data;
  VK_CHECK_RESULT(vmaMapMemory(m_allocator, buffer.allocation, &data));
  m_allocations.push_back(buffer);
  return data;
}
void UIRendererVkNoesis::UnmapVertices() {}
void *UIRendererVkNoesis::MapIndices(uint32_t bytes) {}
void UIRendererVkNoesis::UnmapIndices() {}

} // namespace Expectre