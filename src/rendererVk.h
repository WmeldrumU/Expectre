#ifndef RENDERER_VK_H
#define RENDERER_VK_H

#include <vulkan/vulkan.h>

#include <vector>
#include <optional>
#include <string>
#include <stdexcept>
#include <array>
#include <assimp/Importer.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <stdio.h>



#include <vma/vk_mem_alloc.h>

#include "NsRender/RenderDevice.h"


#include "IRenderer.h"
#include "observer.h"
#include "model.h"
#include "ShaderFileWatcher.h"
#include "IUIRenderer.h"
#include "TextureVk.h"
#include "ToolsVk.h"

#define MAX_CONCURRENT_FRAMES 2

// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define PRESENT_MODE VK_PRESENT_MODE_MAILBOX_KHR
namespace Expectre
{



	struct GeometryBuffer {
		AllocatedBuffer vertices;
		uint32_t vertex_count{ 0 }; // Number of vertices in the buffer
		AllocatedBuffer indices;
		uint32_t index_count{ 0 }; // Number of indices in the buffer
	};

	struct UniformBuffer
	{
		AllocatedBuffer allocated_buffer{};
		// The descriptor set stores the resources bound to the binding points in a shader
		// It connects the binding points of the different shaders with the buffers and images used for those bindings
		VkDescriptorSet descriptorSet{};
		// We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
		uint8_t* mapped{ nullptr };
	};


	class RendererVk : public IRenderer, public InputObserver, public ::Noesis::RenderDevice
	{

	public:
		RendererVk() = delete;
		RendererVk(SDL_Window* window, uint32_t resolution_x, uint32_t resolution_y);
		~RendererVk();

		bool m_enable_validation_layers{ true };
		bool isReady();
		void update(uint64_t delta_t);
		void draw_frame();

	private:

		void create_instance();

		void create_surface();

		void select_physical_device();

		void create_logical_device_and_queues();


		void create_swapchain();

		void create_command_buffers();

		void create_geometry_buffer();

		VkImageView create_swapchain_image_views(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags flags);

		void create_command_pool();

		void create_depth_stencil();

		VkRenderPass create_renderpass(VkDevice device, VkFormat color_format, VkFormat depth_format);

		void create_pipeline();

		void create_descriptor_pool_and_sets();

		VkFramebuffer create_framebuffer(VkDevice device, VkImageView view, VkImageView depth_view = VK_NULL_HANDLE);

		void create_sync_objects();

		void record_draw_commands(VkCommandBuffer command_buffer, uint32_t image_index);

		void create_descriptor_set_layout();

		void create_uniform_buffers();

		void update_uniform_buffer();

		void cleanup_swapchain();

		void on_input_event(const SDL_Event& event) override;
		void create_memory_allocator();
		std::unique_ptr<Model> load_model(std::string dir);

		void create_ui_renderer();

		SDL_Window* m_window{};
		VkInstance m_instance{};
		VkSurfaceKHR m_surface{};

		VkPhysicalDevice m_chosen_phys_device{};
		VkDevice m_device = VK_NULL_HANDLE;

		VkQueue m_graphics_queue = VK_NULL_HANDLE;
		VkQueue m_present_queue = VK_NULL_HANDLE;

		VkSwapchainKHR m_swapchain{};
		std::vector<VkImage> m_swapchain_images{};
		VkFormat m_swapchain_image_format{};
		VkExtent2D m_extent{};
		std::vector<VkFramebuffer> m_swapchain_framebuffers{};
		std::vector<VkImageView> m_swapchain_image_views{};


		VkRenderPass m_render_pass{};
		VkRenderPass m_ui_render_pass{};
		VkPipelineLayout m_pipeline_layout{};
		VkPipelineLayout m_ui_pipeline_layout{};
		VkPipeline m_pipeline{};
		VkPipeline m_ui_pipeline{};
		VkDescriptorPool m_descriptor_pool{};
		// std::vector<VkDescriptorSet> m_descriptor_sets{};
		VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_descriptor_set_layout{ VK_NULL_HANDLE };
		std::vector<const char*> m_validation_layers{ "VK_LAYER_KHRONOS_validation" };

		std::vector<VkSemaphore> m_available_image_semaphores{};
		std::vector<VkSemaphore> m_finished_render_semaphores{};
		std::vector<VkFence> m_in_flight_fences{};

		std::array<struct UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniform_buffers{};
		VkPhysicalDeviceMemoryProperties m_phys_memory_properties{};
		VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
		// VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> m_cmd_buffers;
		bool m_ready = false;

		ToolsVk::ImageAlloc m_depth_image;
		VkImageView m_depth_image_view{};
		VkFormat m_depth_format{};

		VmaAllocator m_allocator{ VK_NULL_HANDLE };

		// Index buffer
		std::vector<Vertex> m_all_vertices{};
		std::vector<uint32_t> m_all_indices{};


		std::vector<std::unique_ptr<Model>> m_models{};
		//std::vector<GeometryBuffer> m_geometry_buffers{};
		GeometryBuffer m_geometry_buffer{};

		TextureVk m_texture{};
		VkSampler m_texture_sampler{};
		VkSurfaceFormatKHR m_surface_format{};
		uint32_t m_graphics_queue_family_index{ UINT32_MAX };
		uint32_t m_present_queue_family_index{ UINT32_MAX };
		uint32_t m_current_frame{ 0 };
		VkDebugUtilsMessengerEXT m_debug_messenger{};

		float m_priority = 1.0f;
		bool m_layers_supported = false;

		struct
		{
			float camera_speed = 1.0f;
			bool moveForward = false;
			bool moveBack = false;
			bool moveLeft = false;
			bool moveRight = false;
			glm::f32vec3 movement_dir = { 0.0f, 0.0f, 0.0f };
			glm::f32vec3 pos = { 0.0f, 1.0f, 2.0f };
			glm::f32vec3 forward_dir = { 0.0f, 0.0f, -1.0f };

		} m_camera{};

		std::unique_ptr<ShaderFileWatcher> m_vert_shader_watcher = nullptr;
		std::unique_ptr<ShaderFileWatcher> m_frag_shader_watcher = nullptr;

		std::unique_ptr<IUIRenderer> m_ui_renderer = nullptr;


		/// From RenderDevice
	//@{
		const Noesis::DeviceCaps& GetCaps() const override;
		Noesis::Ptr<::Noesis::RenderTarget> CreateRenderTarget(const char* label, uint32_t width,
			uint32_t height, uint32_t sampleCount, bool needsStencil) override;
		Noesis::Ptr<Noesis::RenderTarget> CloneRenderTarget(const char* label,
			Noesis::RenderTarget* surface) override;
		Noesis::Ptr<Noesis::Texture> CreateTexture(const char* label, uint32_t width, uint32_t height,
			uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data) override;
		void UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y,
			uint32_t width, uint32_t height, const void* data) override;
		void BeginOffscreenRender() override;
		void EndOffscreenRender() override;
		void BeginOnscreenRender() override;
		void EndOnscreenRender() override;
		void SetRenderTarget(Noesis::RenderTarget* surface) override;
		void BeginTile(Noesis::RenderTarget* surface, const Noesis::Tile& tile) override;
		void EndTile(Noesis::RenderTarget* surface) override;
		void ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles,
			uint32_t numTiles) override;
		void* MapVertices(uint32_t bytes) override;
		void UnmapVertices() override;
		void* MapIndices(uint32_t bytes) override;
		void UnmapIndices() override;
		void DrawBatch(const Noesis::Batch& batch) override;
		//@}

		Noesis::Ptr<Noesis::Texture> WrapTexture(VkImage image, uint32_t width, uint32_t height,
			uint32_t levels, VkFormat format, VkImageLayout layout, bool isInverted, bool hasAlpha);

		std::vector<AllocatedBuffer> m_allocated_buffers{};

	};
}
#endif // RENDERER_VK_H