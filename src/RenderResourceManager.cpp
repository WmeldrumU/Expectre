
#include <RenderResourceManager.h>

#include "TextureManager.h"
#include "ToolsVk.h"
#include <spdlog/spdlog.h>

namespace Expectre {

RenderResourceManager::~RenderResourceManager() {
  destroy_depth_stencil_texture();

  for (auto &[handle, texture_allocation] : m_texture_allocations) {
    if (texture_allocation.view != VK_NULL_HANDLE) {
      vkDestroyImageView(m_device, texture_allocation.view, nullptr);
    }
    if (texture_allocation.image != VK_NULL_HANDLE) {
      vmaDestroyImage(m_allocator, texture_allocation.image,
                      texture_allocation.allocation);
    }
  }

  if (m_transfer_cmd_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device, m_transfer_cmd_pool, nullptr);
  }
  if (m_vertex_buffer.buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(m_allocator, m_vertex_buffer.buffer,
                     m_vertex_buffer.allocation);
  }
  if (m_index_buffer.buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(m_allocator, m_index_buffer.buffer,
                     m_index_buffer.allocation);
  }
}

RenderResourceManager::RenderResourceManager(
    VkDevice device, VkPhysicalDevice phys_device, VmaAllocator allocator,
    uint32_t graphics_queue_family_index, VkQueue queue)
    : m_device(device), m_phys_device(phys_device), m_allocator(allocator),
      m_graphics_queue(queue) {
  create_transfer_command_pool(graphics_queue_family_index);
  m_depth_format = pick_depth_format();
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

TextureAllocation RenderResourceManager::create_texture_allocation(
    uint32_t width, uint32_t height, const uint8_t *pixel_data,
    uint32_t channels, uint32_t mip_levels, VkImageUsageFlags extra_usage,
    VkImageLayout final_layout) {

  VkImageAspectFlags aspect_mask = ToolsVk::choose_aspect_flags(channels);

  const bool uses_color = (aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;
  if (width == 0 || height == 0 || (uses_color && pixel_data == nullptr)) {
    spdlog::warn("Cannot populate texture GPU data: invalid texture inputs");
    return {};
  }

  VkImage image = VK_NULL_HANDLE;
  VkImageView image_view = VK_NULL_HANDLE;
  VmaAllocation image_allocation = VK_NULL_HANDLE;

  // 1. Create GPU texture image
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = mip_levels;
  image_info.arrayLayers = 1;
  image_info.format = uses_color ? VK_FORMAT_R8G8B8A8_SRGB : m_depth_format;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      uses_color ? (VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT | extra_usage)
                 : (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | extra_usage);
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.flags = 0;

  VmaAllocationCreateInfo image_alloc_info{};
  image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VK_CHECK_RESULT(vmaCreateImage(m_allocator, &image_info, &image_alloc_info,
                                 &image, &image_allocation, nullptr));

  if (aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT) {
    // 2. Upload pixel data via staging buffer
    size_t imageSize = static_cast<size_t>(width) * height * channels;

    // Create staging buffer
    AllocatedBuffer staging = ToolsVk::create_buffer(
        m_allocator, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    // Copy pixel data to staging buffer
    void *mapped_data = nullptr;
    VK_CHECK_RESULT(
        vmaMapMemory(m_allocator, staging.allocation, &mapped_data));
    memcpy(mapped_data, pixel_data, imageSize);
    vmaUnmapMemory(m_allocator, staging.allocation);

    // 3. Transition image to transfer destination layout
    ToolsVk::transition_image_layout(
        m_device, m_transfer_cmd_pool, m_graphics_queue, image, aspect_mask,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. Copy from staging buffer to image
    ToolsVk::copy_buffer_to_image(m_device, m_transfer_cmd_pool,
                                  m_graphics_queue, staging.buffer, image,
                                  width, height);

    // 5. Transition to shader readable layout
    ToolsVk::transition_image_layout(
        m_device, m_transfer_cmd_pool, m_graphics_queue, image, aspect_mask,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, final_layout);

    // 6. Cleanup staging buffer
    vmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);
  } else {
    // Depth textures don't need pixel upload, just layout transition
    ToolsVk::transition_image_layout(m_device, m_transfer_cmd_pool,
                                     m_graphics_queue, image, aspect_mask,
                                     VK_IMAGE_LAYOUT_UNDEFINED, final_layout);
  }

  // 7. Create image view via shared helper so aspect selection stays
  // consistent with the rest of the Vulkan utilities.
  image_view = ToolsVk::create_image_view(m_device, image, image_info.format,
                                          aspect_mask);

  TextureAllocation allocation{};
  allocation.image = image;
  allocation.view = image_view;
  allocation.allocation = image_allocation;
  allocation.format = image_info.format;

  return allocation;
}

void RenderResourceManager::create_depth_stencil_texture(uint32_t width,
                                                         uint32_t height) {
  destroy_depth_stencil_texture();

  m_depth_stencil = create_texture_allocation(
      width, height, nullptr, 1 /*channels*/, 1 /*mip levels*/,
      0 /*extra usage*/, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void RenderResourceManager::destroy_depth_stencil_texture() {
  if (m_depth_stencil.view != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, m_depth_stencil.view, nullptr);
    m_depth_stencil.view = VK_NULL_HANDLE;
  }
  if (m_depth_stencil.image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator, m_depth_stencil.image,
                    m_depth_stencil.allocation);
    m_depth_stencil.image = VK_NULL_HANDLE;
    m_depth_stencil.allocation = VK_NULL_HANDLE;
  }
  m_depth_stencil.format = VK_FORMAT_UNDEFINED;
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
  m_mesh_allocations.push_back(alloc);

  // Destroy staging
  vmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);
}

std::optional<TextureAllocation>
RenderResourceManager::upload_texture_to_gpu(TextureHandle texture_handle) {
  if (!texture_handle) {
    spdlog::warn("Attempted to upload invalid texture handle to GPU");
    return {};
  }

  if (m_texture_allocations.find(texture_handle) !=
      m_texture_allocations.end()) {
    return {};
  }

  auto &texture = TextureManager::Instance().get_texture(texture_handle);
  if (texture.width == 0 || texture.height == 0 || texture.data == nullptr) {
    spdlog::warn("Attempted to upload empty texture to GPU");
    return {};
  }

  const uint32_t texture_map_index = m_texture_allocations.size();

  m_texture_allocations[texture_handle] = create_texture_allocation(
      texture.width, texture.height, texture.data, texture.channels);
  m_texture_allocations[texture_handle].texture_map_idx = texture_map_index;
  return m_texture_allocations[texture_handle];
}

void RenderResourceManager::upload_renderable_to_gpu(
    const RenderableInfo &info) {

  const auto &mesh = MeshManager::Instance().get_mesh(info.mesh);

  // upload_texture_to_gpu(info.material.normal);
  // upload_texture_to_gpu(info.material.metallic);
  // upload_texture_to_gpu(info.material.roughness);
}

} // namespace Expectre
