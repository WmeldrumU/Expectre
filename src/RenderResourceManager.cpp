
#include <RenderResourceManager.h>

#include "ToolsVk.h"
#include <spdlog/spdlog.h>

namespace Expectre {

void RenderResourceManager::create_vertex_buffer(uint32_t size_bytes) {
  auto buf = ToolsVk::create_buffer(
      m_allocator, size_bytes,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_vertex_buffer.buffer = buf.buffer;
  m_vertex_buffer.allocation = buf.allocation;
  m_vertex_buffer.byte_offset = 0;
  m_vertex_buffer.byte_size = size_bytes;
}

void RenderResourceManager::create_index_buffer(uint32_t size_bytes) {
  auto buf = ToolsVk::create_buffer(
      m_allocator, size_bytes,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_index_buffer.buffer = buf.buffer;
  m_index_buffer.allocation = buf.allocation;
  m_index_buffer.byte_offset = 0;
  m_index_buffer.byte_size = size_bytes;
}

void RenderResourceManager::upload_mesh_to_gpu(const Mesh &mesh,
                                               VkCommandPool cmd_pool) {
  if (mesh.vertices.empty() || mesh.indices.empty()) {
    spdlog::warn("Attempted to upload empty mesh to GPU");
    return;
  }

  // Sizes
  uint32_t vertex_bytes =
      static_cast<uint32_t>(sizeof(Vertex) * mesh.vertices.size());
  uint32_t index_bytes =
      static_cast<uint32_t>(sizeof(uint32_t) * mesh.indices.size());

  // Alignments:
  // - vkCmdBindIndexBuffer offset must be multiple of index type size AND 4.
  // For uint32 it's 4.
  // - Keep vertex staging region aligned to 4 so index region starts aligned.
  vertex_bytes = AlignUp(vertex_bytes, 4);
  index_bytes = AlignUp(index_bytes, 4);

  // Ensure capacity in destination buffers (you’ll eventually want
  // growth/realloc here)
  assert(m_vertex_buffer.buffer && m_index_buffer.buffer);
  assert(m_vertex_buffer.byte_offset + vertex_bytes <=
         m_vertex_buffer.byte_size);
  m_index_buffer.byte_offset = AlignUp(m_index_buffer.byte_offset, 4);
  assert(m_index_buffer.byte_offset + index_bytes <= m_index_buffer.byte_size);

  // Record starting offsets BEFORE increment
  const uint32_t vertex_dst_start_bytes = m_vertex_buffer.byte_offset;
  const uint32_t index_dst_start_bytes = m_index_buffer.byte_offset;

  // Single staging buffer containing [vertices][indices]
  const uint32_t staging_bytes = vertex_bytes + index_bytes;

  AllocatedBuffer staging = ToolsVk::create_buffer(
      m_allocator, staging_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  void *mapped = nullptr;
  VK_CHECK_RESULT(vmaMapMemory(m_allocator, staging.allocation, &mapped));
  auto *dst = static_cast<uint8_t *>(mapped);

  std::memcpy(dst, mesh.vertices.data(), sizeof(Vertex) * mesh.vertices.size());
  std::memcpy(dst + vertex_bytes, mesh.indices.data(),
              sizeof(uint32_t) * mesh.indices.size());

  vmaUnmapMemory(m_allocator, staging.allocation);

  // Copy vertices
  VkBufferCopy vcopy{};
  vcopy.srcOffset = 0;
  vcopy.dstOffset = static_cast<VkDeviceSize>(vertex_dst_start_bytes);
  vcopy.size = static_cast<VkDeviceSize>(vertex_bytes);

  ToolsVk::copy_buffer(m_device, cmd_pool, m_graphics_queue, staging.buffer,
                       m_vertex_buffer.buffer, vcopy);

  // Copy indices
  VkBufferCopy icopy{};
  icopy.srcOffset = static_cast<VkDeviceSize>(vertex_bytes);
  icopy.dstOffset = static_cast<VkDeviceSize>(index_dst_start_bytes);
  icopy.size = static_cast<VkDeviceSize>(index_bytes);

  ToolsVk::copy_buffer(m_device, cmd_pool, m_graphics_queue, staging.buffer,
                       m_index_buffer.buffer, icopy);

  // Advance allocators
  m_vertex_buffer.byte_offset += vertex_bytes;
  m_index_buffer.byte_offset += index_bytes;

  // Track allocation in ELEMENT offsets (what vkCmdDrawIndexed expects)
  MeshAllocation alloc{};
  alloc.vertex_count = static_cast<uint32_t>(mesh.vertices.size());
  alloc.index_count = static_cast<uint32_t>(mesh.indices.size());
  alloc.vertex_offset = vertex_dst_start_bytes / sizeof(Vertex);
  alloc.index_offset = index_dst_start_bytes / sizeof(uint32_t);
  alloc.material = mesh.material;
  m_mesh_allocations.push_back(alloc);

  // Destroy staging
  vmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);
}

void RenderResourceManager::upload_texture_to_gpu(const Texture &texture) {
  // Early return if texture is empty
  if (texture.width == 0 || texture.height == 0 || texture.data == nullptr) {
    spdlog::warn("Attempted to upload empty texture to GPU");
    return;
  }

  // Calculate image size (assuming RGBA format for GPU upload)
  size_t imageSize = texture.width * texture.height * 4;

  // Create staging buffer
  AllocatedBuffer staging = ToolsVk::create_buffer(
      m_allocator, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  // Upload image data to the staging buffer
  void *data;
  VK_CHECK_RESULT(vmaMapMemory(m_allocator, staging.allocation, &data));
  memcpy(data, texture.data, imageSize);
  vmaUnmapMemory(m_allocator, staging.allocation);

  // Transfer layout and copy data
  TextureVk::transition_image_layout(m_device, m_cmd_pool, m_graphics_queue,
                                     m_texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  ToolsVk::copy_buffer_to_image(m_device, m_cmd_pool, m_graphics_queue,
                                staging.buffer, m_texture.image, texture.width,
                                texture.height);

  // Final layout transition
  TextureVk::transition_image_layout(m_device, m_cmd_pool, m_graphics_queue,
                                     m_texture.image, VK_FORMAT_R8G8B8A8_SRGB,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Cleanup staging buffer
  vmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);
}

void RenderResourceManager::upload_material_to_gpu(
    const Material &material, VkCommandPool cmd_pool) {
  // if (mesh.vertices.empty() || mesh.indices.empty()) {
  //   spdlog::warn("Attempted to upload empty mesh to GPU");
  //   return;
  // }
      TextureManager::Instance().g
      const auto& albedo = material.albedo.texture_id

  auto it = m_material_allocations.find()
}

} // namespace Expectre
