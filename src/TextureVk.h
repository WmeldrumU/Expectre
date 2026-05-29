#ifndef TEXTURE_VK_H
#define TEXTURE_VK_H
#include <exception>
#include <spdlog/spdlog.h>
#include <stb.h>
#include <stb_image.h>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "ToolsVk.h"

namespace Expectre {

class TextureVk {
public:
  VkImage image;
  VkImageView view;
  VmaAllocation allocation;
  VkImageCreateInfo image_info;
  VkImageViewCreateInfo view_info;
  VkImageLayout layout;

  static VkImageAspectFlags choose_aspect(VkFormat format) {
    if (format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT)
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else if (format == VK_FORMAT_D32_SFLOAT)
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    else
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }

  static void transition_image_layout(VkDevice device, VkCommandPool cmd_pool,
                                      VkQueue graphics_queue, VkImage image,
                                      VkFormat format, VkImageLayout old_layout,
                                      VkImageLayout new_layout) {
    VkCommandBuffer cmd_buffer =
        ToolsVk::begin_single_time_commands(device, cmd_pool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = choose_aspect(format);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags dest_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
               new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
      // No previous accesses to wait on
      barrier.srcAccessMask = 0;

      // We�re going to use the image as a depth/stencil attachment (read for
      // tests + write)
      barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      // Nothing before it; make it available early in the pipeline
      source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

      // Depth/stencil tests happen here (and may also write)
      dest_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
      throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cmd_buffer, source_stage, dest_stage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    ToolsVk::end_single_time_commands(device, cmd_pool, cmd_buffer,
                                      graphics_queue);
  }

  static TextureVk create_texture_from_file(VkDevice device,
                                            VkCommandPool cmd_pool,
                                            VkQueue graphics_queue,
                                            VmaAllocator allocator,
                                            const std::string &dir) {
    int tex_width, tex_height, tex_channels;
    stbi_uc *pixels = stbi_load(dir.c_str(), &tex_width, &tex_height,
                                &tex_channels, 4); // Force RGBA

    if (!pixels) {
      spdlog::error("Failed to load texture from '{}'", dir);
      std::terminate();
    }
    TextureVk tex = create_texture(device, cmd_pool, graphics_queue, allocator,
                                   pixels, tex_width, tex_height);
    stbi_image_free(pixels);
    return tex;
  }

private:
};

} // Namespace Expectre
#endif // TEXTURE VK_H