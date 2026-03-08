#ifndef RENDER_CONTEXT_VK
#define RENDER_CONTEXT_VK


#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>              // for SDL_Window
#include <vma/vk_mem_alloc.h>      // for VmaAllocator
#include <memory>                  // for std::shared_ptr

#include "IRenderer.h"

namespace Expectre
{

#define RESOLUTION_X 1280
#define RESOLUTION_Y 720

	class RenderContextVk
	{
	public:
		RenderContextVk() = delete;
		RenderContextVk(SDL_Window* window);
		~RenderContextVk();

		const VkDevice& get_device() { return m_device; }
		const VkPhysicalDevice& get_phys_device() { return m_physical_device; }
		uint32_t graphics_queue_index() { return m_graphics_queue_index; }
		uint32_t present_queue_index() { return m_present_queue_index; }
		const VmaAllocator& get_allocator() { return m_allocator; }
		const VkSurfaceKHR& get_surface() { return m_surface; }
		void UpdateAndRender(uint64_t delta_time, SceneObject& object);
		bool is_ready() { return m_ready; }

	private:
		void create_instance();
		void create_device();
		void create_surface();
		void create_memory_allocator();


		VkInstance m_instance;
		SDL_Window* m_window{};
		VkSurfaceKHR m_surface{};
		VmaAllocator m_allocator{};
		VkPhysicalDevice m_physical_device{};
		VkDevice m_device = VK_NULL_HANDLE;
		std::shared_ptr<IRenderer> m_renderer = nullptr;

		// make queues/indeces part of a device class?
		VkCommandPool m_cmd_pool;

		uint32_t m_graphics_queue_index{ UINT32_MAX };
		uint32_t m_present_queue_index{ UINT32_MAX };
		VkQueue m_graphics_queue{};
		VkQueue m_present_queue{};
		float m_priority = 1.0;

		bool m_ready{ false };
	};

} // namespace Expectre

#endif //RENDER_CONTEXT_VK