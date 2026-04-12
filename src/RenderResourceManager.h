
#ifndef RENDER_RESOURCE_MANAGER_H
#define RENDER_RESOURCE_MANAGER_H

#include "Mesh.h"
#include "MeshManager.h"
#include "RenderableInfo.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <unordered_map>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
namespace Expectre {

struct MeshAllocation {
  uint32_t vertex_offset; // in vertices (not bytes)
  uint32_t vertex_count;
  uint32_t index_offset; // in indices (not bytes)
  uint32_t index_count;
};

struct MaterialAllocation {
  // Indices of the textures in the
  // sampler2D array
  uint32_t albedo_index;
  uint32_t normal_index;
  uint32_t metallic_index;
  uint32_t roughness_index;
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

struct TextureAllocation {
  VkImage image = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
};

class RenderResourceManager {
public:
  RenderResourceManager() = delete;
  RenderResourceManager(VkDevice device, VkPhysicalDevice phys_device,
                        VmaAllocator allocator,
                        uint32_t graphics_queue_family_index, VkQueue queue);

  ~RenderResourceManager();

  void create_vertex_buffer(uint32_t size_bytes);
  void create_index_buffer(uint32_t size_bytes);
  const IndexBuffer &get_index_buffer() { return m_index_buffer; }
  const VertexBuffer &get_vertex_buffer() { return m_vertex_buffer; }
  void upload_mesh_to_gpu(const Mesh &mesh);
  void upload_texture_to_gpu(TextureHandle texture_handle);
  void create_depth_stencil_texture(uint32_t width, uint32_t height);
  void destroy_depth_stencil_texture();

  void upload_material_to_gpu(const Material &material);

  const std::vector<MeshAllocation> &get_mesh_allocations() const {
    return m_mesh_allocations;
  }

  std::optional<std::reference_wrapper<const TextureAllocation>>
  get_texture_allocation(TextureHandle texture_handle) const {
    auto it = m_texture_allocations.find(texture_handle);
    if (it != m_texture_allocations.end()) {
      return std::cref(it->second);
    }
    return std::nullopt;
  }

  const TextureAllocation &get_depth_stencil_texture() const {
    return m_depth_stencil;
  }

  void upload_renderable_to_gpu(const RenderableInfo &info);

private:
  // Aligns v up to the nearest multiple of a (which must be a power of 2)
  // e.g., AlignUp(5, 4) returns 8, AlignUp(12, 8) returns 16
  // if v is already a multiple of a, it returns v (e.g., AlignUp(8, 4) returns
  // 8) if a is not a power of 2, this will produce incorrect results
  static uint32_t AlignUp(uint32_t v, uint32_t a) {
    return (v + (a - 1)) & ~(a - 1);
  }

  void create_transfer_command_pool(uint32_t graphics_queue_family_index);

  VkFormat pick_depth_format() const {
    constexpr VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat fmt : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(m_phys_device, fmt, &props);
      if (props.optimalTilingFeatures &
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        return fmt;
      }
    }
    throw std::runtime_error("No supported depth-stencil format found");
  }

  TextureAllocation create_texture_allocation(
      uint32_t width, uint32_t height, const uint8_t *pixel_data,
      uint32_t channels = 4, uint32_t mip_levels = 1,
      VkImageUsageFlags extra_usage = 0,
      VkImageLayout final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  VkDevice m_device = VK_NULL_HANDLE;
  VkPhysicalDevice m_phys_device = VK_NULL_HANDLE;
  VmaAllocator m_allocator = VK_NULL_HANDLE;
  VkQueue m_graphics_queue = VK_NULL_HANDLE;
  // Transfer command pool (owned by RendererVk, used for GPU uploads)
  VkCommandPool m_transfer_cmd_pool = VK_NULL_HANDLE;
  VkFormat m_depth_format = VK_FORMAT_UNDEFINED;

  VertexBuffer m_vertex_buffer{};
  IndexBuffer m_index_buffer{};
  std::vector<MeshAllocation> m_mesh_allocations;
  TextureAllocation m_depth_stencil{};
  std::unordered_map<TextureHandle, TextureAllocation> m_texture_allocations;
  // map that provide the indices of the textures within the shader's Sampler2D
  // array
  // std::unordered_map<TextureHandle, uint32_t> m_texture_allocation;
};
} // namespace Expectre

#endif // RENDER_RESOURCE_MANAGER_H