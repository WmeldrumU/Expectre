namespace Expectre
{

#define RESOLUTION_X 1280
#define RESOLUTION_Y 720

	static class RenderContextVk
	{
	public:
		RenderContextVk();
		~RenderContextVk();

		const VkDevice& get_device();
		const VkPhysicalDevice& get_phys_device();
		uint32_t graphics_queue_index() { return m_graphics_queue_index; }
		uint32_t present_queue_index() { return m_present_queue_index; }

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
		std::shared_ptr<RendererVk> m_renderer;

		// make queues/indeces part of a device class?
		VkCommandPool m_cmd_pool;

		uint32_t m_graphics_queue_index{ UINT32_MAX };
		uint32_t m_present_queue_index{ UINT32_MAX };
		VkQueue m_graphics_queue{};
		VkQueue m_present_queue{};
		float m_priority = 1.0;
	};

} // namespace Expectre
