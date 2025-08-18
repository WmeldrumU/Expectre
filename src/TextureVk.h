#include <spdlog/spdlog.h>

#ifndef TEXTURE_VK_H
#define TEXTURE_VK_H
#include <spdlog/spdlog.h>
#include <exception>
#include <stb_image.h>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <stb.h>

#include "VkTools.h"

namespace Expectre
{


	class TextureVk
	{
	public:
		VkImage image;
		VkImageView view;
		VmaAllocation allocation;

		static void transition_image_layout(VkDevice device, VkCommandPool cmd_pool, VkQueue graphics_queue,
			VkCommandBuffer cmd_buffer, VkImage image, VkFormat format,
			VkImageLayout old_layout, VkImageLayout new_layout)
		{
			VkCommandBuffer cmd_buffer = ToolsVk::begin_single_time_commands(device, cmd_pool);

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = old_layout;
			barrier.newLayout = new_layout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			VkPipelineStageFlags source_stage;
			VkPipelineStageFlags dest_stage;

			if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			{
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			else
			{
				throw std::invalid_argument("unsupported layout transition!");
			}

			vkCmdPipelineBarrier(
				cmd_buffer,
				source_stage, dest_stage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			ToolsVk::end_single_time_commands(device, cmd_pool, cmd_buffer, graphics_queue);
		}

		static const TextureVk create_texture_from_file(VmaAllocator allocator, std::string dir)
		{
			VkImage image;
			VkImageView image_view;
			VmaAllocation image_allocation;

			int tex_width, tex_height, tex_channels;
			stbi_uc* pixels = stbi_load(dir.c_str(),
				&tex_width, &tex_height,
				&tex_channels, 0);

			if (!pixels)
			{
				spdlog::error("Failed to load texture!");
				std::terminate();
			}

			VkDeviceSize image_size = tex_width * tex_height * tex_channels;

			// 1. Create staging buffer (host-visible)
			AllocatedBuffer staging = ToolsVk::create_buffer(allocator,
				image_size,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VMA_MEMORY_USAGE_CPU_ONLY);

			// 2. Upload image data to the staging buffer
			void* data;
			VK_CHECK_RESULT(vmaMapMemory(allocator, staging.allocation, &data));
			memcpy(data, pixels, static_cast<size_t>(image_size));
			vmaUnmapMemory(allocator, staging.allocation);

			stbi_image_free(pixels);

			// 3. Create GPU texture image (device-local)
			VkImageCreateInfo image_info = {};
			image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image_info.imageType = VK_IMAGE_TYPE_2D;
			image_info.extent.width = static_cast<uint32_t>(tex_width);
			image_info.extent.height = static_cast<uint32_t>(tex_height);
			image_info.extent.depth = 1;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
			image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
			image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image_info.samples = VK_SAMPLE_COUNT_1_BIT;
			image_info.flags = 0;

			VmaAllocationCreateInfo image_alloc_info = {};
			image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

			VK_CHECK_RESULT(vmaCreateImage(allocator,
				&image_info,
				&image_alloc_info,
				&image,
				&image_allocation,
				nullptr));

			// 4. Transfer layout and copy data
			transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			copy_buffer_to_image(staging.buffer, image,
				static_cast<uint32_t>(tex_width),
				static_cast<uint32_t>(tex_height));

			transition_image_layout(image, VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			// 5. Cleanup staging buffer
			vmaDestroyBuffer(m_allocator, staging.buffer, staging.allocation);

			// 6. Create image view
			VkImageViewCreateInfo view_info{};
			view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view_info.image = image;
			view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
			view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			view_info.subresourceRange.baseMipLevel = 0;
			view_info.subresourceRange.levelCount = 1;
			view_info.subresourceRange.baseArrayLayer = 0;
			view_info.subresourceRange.layerCount = 1;

			VK_CHECK_RESULT(vkCreateImageView(m_device,
				&view_info,
				nullptr, &image_view));

			// Package the texure data
			return TextureVk{
				.image = image,
				.image_view = image_view,
				.allocation = image_allocation,
				.image_create_info = image_info,
				.image_view_info = view_info,
			};
		}

	private:
	};
}
#endif // TEXTURE VK_H