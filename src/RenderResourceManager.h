
#ifndef RENDER_RESOURCE_MANAGER_H
#define RENDER_RESOURCE_MANAGER_H

#include "Mesh.h"

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Expectre {

struct MeshAllocation {
  uint32_t vertex_offset; // in vertices (not bytes)
  uint32_t vertex_count;
  uint32_t index_offset; // in indices (not bytes)
  uint32_t index_count;
};

struct VertexBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  uint32_t byte_offset = 0; // current "end" of used region in bytes
  uint32_t byte_size = 0;   // total buffer size in bytes
};

struct IndexBuffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  uint32_t byte_offset = 0;
  uint32_t byte_size = 0;
};

class RenderResourceManager {
public:
  RenderResourceManager() = delete;
  RenderResourceManager(VkDevice device, VmaAllocator allocator, VkQueue queue)
      : m_device(device), m_allocator(allocator), m_graphics_queue(queue) {}

  void create_vertex_buffer(uint32_t size_bytes);
  void create_index_buffer(uint32_t size_bytes);
  const IndexBuffer &get_index_buffer() { return m_index_buffer; }
  const VertexBuffer &get_vertex_buffer() { return m_vertex_buffer; }
  void upload_mesh_to_gpu(const Mesh &mesh, VkCommandPool cmd_pool);

  const std::vector<MeshAllocation> &get_mesh_allocations() const {
    return m_mesh_allocations;
  }

private:
  // Aligns v up to the nearest multiple of a (which must be a power of 2)
  // e.g., AlignUp(5, 4) returns 8, AlignUp(12, 8) returns 16
  // if v is already a multiple of a, it returns v (e.g., AlignUp(8, 4) returns
  // 8) if a is not a power of 2, this will produce incorrect results
  static uint32_t AlignUp(uint32_t v, uint32_t a) {
    return (v + (a - 1)) & ~(a - 1);
  }

  VkDevice m_device = VK_NULL_HANDLE;
  VmaAllocator m_allocator = VK_NULL_HANDLE;
  VkQueue m_graphics_queue = VK_NULL_HANDLE;

  VertexBuffer m_vertex_buffer{};
  IndexBuffer m_index_buffer{};
  std::vector<MeshAllocation> m_mesh_allocations;
};
} // namespace Expectre

#endif // RENDER_RESOURCE_MANAGER_H