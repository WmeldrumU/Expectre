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

	struct GeometryBuffer
	{
		AllocatedBuffer vertices;
		uint32_t vertex_count{ 0 }; // Number of vertices in the buffer
		AllocatedBuffer indices;
		uint32_t index_count{ 0 }; // Number of indices in the buffer
	};

	struct UniformBuffer
	{
		AllocatedBuffer allocated_buffer{};
		// A descriptor set in Vulkan is a GPU-side object that binds your shader to actual resources
		VkDescriptorSet descriptorSet{};
		// We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
		uint8_t* mapped{ nullptr };
	};

	class RendererVk : public IRenderer, public InputObserver
	{

	public:
		RendererVk() = delete;
		RendererVk(RenderContextVk& context);
		~RendererVk();

		bool isReady();
		void update(uint64_t delta_t);
		void draw_frame();

	private:

		void create_logical_device_and_queues();

		void create_swapchain();

		VkCommandBuffer create_command_buffer(VkDevice device, VkCommandPool command_pool);

		void create_geometry_buffer();

		VkImageView create_swapchain_image_views(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags flags);

		VkCommandPool create_command_pool(VkDevice device, uint32_t graphics_queue_family_index);

		VkRenderPass create_renderpass(VkDevice device, VkFormat color_format, VkFormat depth_format, bool is_presenting_pass);

		VkPipeline create_pipeline(VkDevice device, VkRenderPass renderpass, VkPipelineLayout pipeline_layout);

		VkDescriptorPool create_descriptor_pool(VkDevice device, std::vector<VkDescriptorPoolSize> pool_sizes, uint32_t num_sets);

		VkDescriptorSet create_descriptor_set(VkDevice device, VkDescriptorPool descriptor_pool, VkDescriptorSetLayout descriptor_layout, VkBuffer buffer, VkImageView image_view,
			VkSampler sampler);

		VkFramebuffer create_framebuffer(VkDevice device, VkImageView view, VkImageView depth_view = VK_NULL_HANDLE);

		void create_sync_objects();

		void record_draw_commands(VkCommandBuffer command_buffer, uint32_t image_index);

		VkPipelineLayout create_pipeline_layout(VkDevice device, VkDescriptorSetLayout descriptor_set_layout);

		VkDescriptorSetLayout create_descriptor_set_layout(const std::vector<VkDescriptorSetLayoutBinding>& layout_bindings);

		UniformBuffer create_uniform_buffer(VmaAllocator allocator, VkDeviceSize buffer_size);

		void update_uniform_buffer();

		void cleanup_swapchain();

		void on_input_event(const SDL_Event& event) override;
		void create_memory_allocator();
		std::unique_ptr<Model> load_model(std::string dir);

		VkQueue m_graphics_queue = VK_NULL_HANDLE;
		VkQueue m_present_queue = VK_NULL_HANDLE;

		VkSwapchainKHR m_swapchain{};
		std::vector<VkImage> m_swapchain_images{};
		VkFormat m_swapchain_image_format{};
		VkExtent2D m_extent{};
		std::vector<VkFramebuffer> m_swapchain_framebuffers{};
		std::vector<VkImageView> m_swapchain_image_views{};

		VkRenderPass m_render_pass{};
		VkPipelineLayout m_pipeline_layout{};
		VkPipelineLayout m_ui_pipeline_layout{};
		VkPipeline m_pipeline{};
		VkPipeline m_ui_pipeline{};
		VkDescriptorPool m_descriptor_pool{};
		VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_descriptor_set_layout{ VK_NULL_HANDLE };

		std::vector<VkSemaphore> m_available_image_semaphores{};
		std::vector<VkSemaphore> m_finished_render_semaphores{};
		std::vector<VkFence> m_in_flight_fences{};

		std::array<struct UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniform_buffers{};
		VkPhysicalDeviceMemoryProperties m_phys_memory_properties{};
		VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
		// VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
		std::array<VkCommandBuffer, MAX_CONCURRENT_FRAMES> m_cmd_buffers;
		bool m_ready = false;

		TextureVk m_depth_stencil;

		// Index buffer
		std::vector<Vertex> m_all_vertices{};
		std::vector<uint32_t> m_all_indices{};

		std::vector<std::unique_ptr<Model>> m_models{};
		// std::vector<GeometryBuffer> m_geometry_buffers{};
		GeometryBuffer m_geometry_buffer{};

		TextureVk m_texture{};
		VkSampler m_texture_sampler{};
		VkSurfaceFormatKHR m_surface_format{};

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

		RenderContextVk m_context;
	};

}
#endif // RENDERER_VK_H