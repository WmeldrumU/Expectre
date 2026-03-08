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
		VkPipelineCache m_pipeline_cache{};
		std::function<void(VkCommandBuffer)> queue_submit_function = nullptr;
		VmaAllocator allocator;
	};
}
#endif