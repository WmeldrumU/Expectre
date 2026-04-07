
#include <RenderResourceManager.h>

#include "TextureManager.h"
#include "ToolsVk.h"
#include <spdlog/spdlog.h>

namespace Expectre {

RenderResourceManager::RenderResourceManager(
    VkDevice device, VkPhysicalDevice phys_device, VmaAllocator allocator,
    uint32_t graphics_queue_family_index, VkQueue queue)
    : m_device(device), m_phys_device(phys_device), m_allocator(allocator),
      m_graphics_queue(queue) {
  create_transfer_command_pool(graphics_queue_family_index);
}

void RenderResourceManager::create_transfer_command_pool(
    uint32_t graphics_queue_family_index) {
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = graphics_queue_family_index;

  VK_CHECK_RESULT(
      vkCreateCommandPool(m_device, &pool_info, nullptr, &m_transfer_cmd_pool));
}

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

void RenderResourceManager::populate_texture_with_gpu_data(
    Texture &texture, uint32_t mip_levels, VkFormat texture_format,
    VkImageAspectFlags aspect_mask, VkImageUsageFlags extra_usage,
    VkImageLayout final_layout) {
  if (texture.width == 0 || texture.height == 0 || texture.data == nullptr) {
    spdlog::warn("Cannot populate texture GPU data: invalid CPU data");
    return;
  }

  VkImage image = VK_NULL_HANDLE;
  VkImageView image_view = VK_NULL_HANDLE;
  VmaAllocation image_allocation = VK_NULL_HANDLE;

  // 1. Create GPU texture image
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = texture.width;
  image_info.extent.height = texture.height;
  image_info.extent.depth = 1;
  image_info.mipLevels = mip_levels;
  image_info.arrayLayers = 1;
  image_info.format = texture_format;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | extra_usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.flags = 0;

  VmaAllocationCreateInfo image_alloc_info{};
  image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VK_CHECK_RESULT(vmaCreateImage(m_allocator, &image_info, &image_alloc_info,
                                 &image, &image_allocation, nullptr));

  if (aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT) {
    // 2. Upload pixel data via staging buffer
    size_t imageSize = texture.width * texture.height * 4;

    // Create staging buffer
    AllocatedBuffer staging = ToolsVk::create_buffer(
        m_allocator, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Copy pixel data to staging buffer
    void *mapped_data = nullptr;
    VK_CHECK_RESULT(vmaMapMemory(m_allocator, staging.allocation, &mapped_data));
    memcpy(mapped_data, texture.data, imageSize);
    vmaUnmapMemory(m_allocator, staging.allocation);

    // 3. Transition image to transfer destination layout
    ToolsVk::transition_image_layout(m_device, m_transfer_cmd_pool,
                                     m_graphics_queue, image, texture_format,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. Copy from staging buffer to image
    ToolsVk::copy_buffer_to_image(m_device, m_transfer_cmd_pool, m_graphics_queue,
                                  staging.buffer, image, texture.width,
                                  texture.height);

    // 5. Transition to shader readable layout
    ToolsVk::transition_image_layout(m_device, m_transfer_cmd_pool,
                                     m_graphics_queue, image, texture_format,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     final_layout);

    // 6. Cleanup staging buffer
    vmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);
  } else {
    // Depth textures don't need pixel upload, just layout transition
    ToolsVk::transition_image_layout(
        m_device, m_transfer_cmd_pool, m_graphics_queue, image, texture_format,
        VK_IMAGE_LAYOUT_UNDEFINED, final_layout);
  }

  // 7. Create image view
  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = texture_format;
  view_info.subresourceRange.aspectMask = aspect_mask;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  VK_CHECK_RESULT(vkCreateImageView(m_device, &view_info, nullptr, &image_view));

  // 8. Populate the texture object with GPU resources
  texture.image = image;
  texture.view = image_view;
  texture.allocation = image_allocation;
}

void RenderResourceManager::create_depth_stencil_texture(Texture &texture,
                                                         uint32_t width,
                                                         uint32_t height) {
  // Set dimensions before population
  texture.width = width;
  texture.height = height;

  // Find compatible depth format
  VkFormat depth_format = ToolsVk::find_depth_format(m_phys_device);
  texture.format = depth_format;

  // Populate the texture using the helper with depth-specific parameters
  populate_texture_with_gpu_data(
      texture, 1, depth_format,
      VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void RenderResourceManager::upload_mesh_to_gpu(const Mesh &mesh) {
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

  ToolsVk::copy_buffer(m_device, m_transfer_cmd_pool, m_graphics_queue,
                       staging.buffer, m_vertex_buffer.buffer, vcopy);

  // Copy indices
  VkBufferCopy icopy{};
  icopy.srcOffset = static_cast<VkDeviceSize>(vertex_bytes);
  icopy.dstOffset = static_cast<VkDeviceSize>(index_dst_start_bytes);
  icopy.size = static_cast<VkDeviceSize>(index_bytes);

  ToolsVk::copy_buffer(m_device, m_transfer_cmd_pool, m_graphics_queue,
                       staging.buffer, m_index_buffer.buffer, icopy);

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

void RenderResourceManager::upload_texture_to_gpu(Texture &texture) {
  // Early return if texture is empty
  if (texture.width == 0 || texture.height == 0 || texture.data == nullptr) {
    spdlog::warn("Attempted to upload empty texture to GPU");
    return;
  }

  if (texture.image == VK_NULL_HANDLE || texture.view == VK_NULL_HANDLE) {
    populate_texture_with_gpu_data(texture);
  }
}

void RenderResourceManager::upload_material_to_gpu(const Material &material) {
  // if (mesh.vertices.empty() || mesh.indices.empty()) {
  //   spdlog::warn("Attempted to upload empty mesh to GPU");
  //   return;
  // }
  // const auto& albedo = material.albedo.texture_id

  // auto it = m_material_allocations.find()

  auto &tex_mgr = TextureManager::Instance();
  // albedo
  auto &albedo = tex_mgr.get_texture(material.albedo);
  upload_texture_to_gpu(albedo);
}

} // namespace Expectre
