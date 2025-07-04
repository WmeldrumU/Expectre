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
#define MAX_CONCURRENT_FRAMES 2

// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define PRESENT_MODE VK_PRESENT_MODE_MAILBOX_KHR
namespace Expectre
{

	typedef struct _UniformBuffer
	{
		VmaAllocation allocation{ VK_NULL_HANDLE }; // Allocation handle for the buffer
		VkBuffer buffer{};
		// The descriptor set stores the resources bound to the binding points in a shader
		// It connects the binding points of the different shaders with the buffers and images used for those bindings
		VkDescriptorSet descriptorSet{};
		// We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
		uint8_t* mapped{ nullptr };
	} UniformBuffer;

	struct TextureVk {
		VkImage image;
		VkImageView image_view;
		VmaAllocation allocation;
		VkImageCreateInfo image_create_info;
		VkImageViewCreateInfo image_view_info;
	};

	class Renderer_Vk : public IRenderer, public InputObserver
	{

	public:
		Renderer_Vk();
		~Renderer_Vk();

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

		void create_swapchain_image_views();

		void create_command_pool();

		void create_depth_stencil();

		void create_renderpass();

		void create_pipeline();

		void create_descriptor_pool_and_sets();

		void create_framebuffers();

		void create_sync_objects();

		void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index);

		void create_descriptor_set_layout();

		void create_uniform_buffers();

		void update_uniform_buffer();

		void create_texture_image();

		const TextureVk create_texture_from_file(std::string dir);

		void cleanup_swapchain();

		void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
			VkMemoryPropertyFlags properties, VkBuffer& buffer,
			VkDeviceMemory& buffer_memory);

		void create_image(uint32_t width, uint32_t height,
			VkFormat format, VkImageTiling tiling,
			VkImageUsageFlags usage,
			VkMemoryPropertyFlags properties,
			VkImage& image,
			VkDeviceMemory& image_memory);

		void end_single_time_commands(VkCommandBuffer cmd_buffer);

		void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

		VkCommandBuffer begin_single_time_commands();

		void transition_image_layout(VkImage image,
			VkFormat format, VkImageLayout old_layout,
			VkImageLayout new_layout);

		void create_texture_image_view();

		void create_texture_sampler();

		uint32_t choose_heap_from_flags(const VkMemoryRequirements& memoryRequirements,
			VkMemoryPropertyFlags requiredFlags,
			VkMemoryPropertyFlags prefferedFlags);

		void on_input_event(const SDL_Event& event) override;
		void copy_buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void create_memory_allocator();

		void load_model(std::string dir);

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
		VkExtent2D m_swapchain_extent{};
		std::vector<VkFramebuffer> m_swapchain_framebuffers{};
		std::vector<VkImageView> m_swapchain_image_views{};


		VkRenderPass m_render_pass{};
		VkPipelineLayout m_pipeline_layout{};
		VkPipeline m_pipeline{};
		VkDescriptorPool m_descriptor_pool{};
		// std::vector<VkDescriptorSet> m_descriptor_sets{};
		VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_descriptor_set_layout{ VK_NULL_HANDLE };
		std::vector<const char*> m_validation_layers{ "VK_LAYER_KHRONOS_validation" };

		std::vector<VkSemaphore> m_available_image_semaphores{};
		std::vector<VkSemaphore> m_finished_render_semaphores{};
		std::vector<VkFence> m_in_flight_fences{};

		std::array<UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniform_buffers{};
		VkPhysicalDeviceMemoryProperties m_phys_memory_properties{};
		VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
		// VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> m_cmd_buffers;
		bool m_ready = false;

		VkImage m_depth_image{};
		VkDeviceMemory m_depth_image_memory{};
		VkImageView m_depth_image_view{};
		VkFormat m_depth_format{};

		VmaAllocator m_allocator{ VK_NULL_HANDLE };

		// Index buffer
		std::vector<Vertex> m_all_vertices{};
		std::vector<uint32_t> m_all_indices{};


		std::vector<Model> m_models{};
		//std::vector<GeometryBuffer> m_geometry_buffers{};
		GeometryBuffer m_geometry_buffer{};

		VkImage m_texture_image{};
		VkImageView m_texture_image_view{};
		VkSampler m_texture_sampler{};
		VmaAllocation m_texture_image_allocation{};
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
	};
}
#endif // RENDERER_VK_H