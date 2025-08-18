#ifndef IRENDERERINFO_H
#define IRENDERERINFO_H

#include <vector>
#include <vulkan/vulkan.h>
#include <functional>
#include <vma/vk_mem_alloc.h>

namespace Expectre
{
	struct IRendererInfo
	{
	};

	struct RendererInfoVk : public IRendererInfo
	{
		VkInstance instance{};
		VkDevice device{};
		VkPhysicalDevice phys_device{};
		std::vector<VkCommandBuffer> command_bufffers = std::vector<VkCommandBuffer>();
		uint32_t current_frame{};
		uint32_t queue_family_index{};
		std::function<void(VkCommandBuffer)> queue_submit_function = nullptr;
		VkFormat render_target_format;
		VkExtent2D extent;
		VkCommandPool command_pool;
		VkCommandBuffer command_buffer;
	};
}
#endif