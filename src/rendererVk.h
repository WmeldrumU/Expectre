#ifndef RENDERER_VK_H
#define RENDERER_VK_H
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <string>
#include <stdexcept>
#include <array>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#define MAX_CONCURRENT_FRAMES 2
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000
namespace Expectre
{
    typedef struct _SwapChainBuffers
    {
        VkImage image;
        VkImageView view;
    } SwapChainBuffer;

    typedef struct UniformBuffer
    {
        VkDeviceMemory memory;
        VkBuffer buffer;
        // The descriptor set stores the resources bound to the binding points in a shader
        // It connects the binding points of the different shaders with the buffers and images used for those bindings
        VkDescriptorSet descriptorSet;
        // We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
        uint8_t *mapped{nullptr};
    };

    class Renderer_Vk
    {

    public:
        Renderer_Vk();
        ~Renderer_Vk();

        bool m_enable_validation_layers{true};
        void cleanup();
        bool isReady();
        void draw_frame();

    private:
        void create_instance();

        void enumerate_layers();

        void create_surface();

        void query_swap_chain_support();

        void select_physical_device();

        void create_logical_device_and_queues();

        void get_surface_format();

        void create_swapchain();

        void create_command_buffers();

        void create_vertex_buffer();

        void create_command_pool();

        void create_layouts();

        void prepare_depth();

        void create_renderpass();

        void create_pipeline();

        void prepare_present_cmd_pool_and_buffers();

        void create_descriptor_pool_and_sets();

        void create_framebuffers();

        void create_sync_objects();

        void record_command_buffer();

        void demo_draw_build_cmd();

        void create_buffers_and_images();

        void create_views();

        void select_surface_formats();

        void get_present_mode();

        void create_semaphors_and_fences();

        void create_descriptor_set_layout();

        void create_uniform_buffers();

        void update_uniform_buffer();

        void create_texture_image();

        void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkBuffer &buffer,
                           VkDeviceMemory &buffer_memory);

        void create_image(uint32_t width, uint32_t height,
                          VkFormat format, VkImageTiling tiling,
                          VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkImage &image,
                          VkDeviceMemory &image_memory);

        void end_single_time_commands(VkCommandBuffer cmd_buffer);

        void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

        VkCommandBuffer begin_single_time_commands();

        void transition_image_layout(VkImage image,
                                     VkFormat format, VkImageLayout old_layout,
                                     VkImageLayout new_layout);

        void create_texture_image_view();

        void create_texture_sampler();

        uint32_t choose_heap_from_flags(const VkMemoryRequirements &memoryRequirements,
                                        VkMemoryPropertyFlags requiredFlags,
                                        VkMemoryPropertyFlags prefferedFlags);

        SDL_Window *m_window{};
        VkInstance m_instance{};
        VkSurfaceKHR m_surface{};
        VkSwapchainKHR m_swapchain{};
        VkRenderPass m_render_pass{};
        VkBufferView m_buffer_view{};
        VkImageView m_image_view{};
        VkPipelineLayout m_pipeline_layout{};
        VkPipeline m_pipeline{};
        VkDescriptorPool m_descriptor_pool{};
        // std::vector<VkDescriptorSet> m_descriptor_sets{};
        VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
        std::vector<VkFramebuffer> m_framebuffers{};
        std::vector<const char *> m_layers{"VK_LAYER_KHRONOS_validation"};
        std::vector<VkPhysicalDevice> m_physical_devices;
        std::optional<VkPhysicalDevice> m_chosen_phys_device;
        std::vector<const char *> m_required_instance_extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        std::vector<const char *> m_required_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        std::vector<VkImage> m_swapchain_images{};
        std::vector<SwapChainBuffer> m_swapchain_buffers{};
        // std::vector<VkBuffer> m_swapchain_uniform_buffers{};
        // std::vector<VkDeviceMemory> m_uniform_memories{};
        std::vector<VkSemaphore> m_available_image_semaphores{};
        std::vector<VkSemaphore> m_finished_render_semaphores{};
        std::vector<VkFence> m_in_flight_fences{};

        std::array<UniformBuffer, MAX_CONCURRENT_FRAMES> m_uniform_buffers{};
        VkPhysicalDeviceMemoryProperties m_phys_memory_properties{};
        VkDevice m_device = VK_NULL_HANDLE;
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
        VkCommandPool m_present_cmd_pool = VK_NULL_HANDLE;
        // VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_cmd_buffers;
        bool m_ready = false;

        struct
        {
            VkFormat format;

            VkImage image;
            VkMemoryAllocateInfo mem_alloc;
            VkDeviceMemory mem;
            VkImageView view;
        } m_depth;

        // Index buffer
        struct
        {
            VkDeviceMemory memory{VK_NULL_HANDLE};
            VkBuffer buffer;
            uint32_t count{0};
        } m_indices;

        // Vertex buffer and attributes
        struct
        {
            VkDeviceMemory memory{VK_NULL_HANDLE}; // Handle to the device memory for this buffer
            VkBuffer buffer;                       // Handle to the Vulkan buffer object that the memory is bound to
        } m_vertices;

        VkImage m_texture_image;
        VkImageView m_texture_image_view;
        VkSampler m_texture_sampler;
        VkDeviceMemory m_texture_image_memory;
        VkSurfaceFormatKHR m_surface_format{};
        uint32_t m_graphics_queue_family_index{UINT32_MAX};
        uint32_t m_present_queue_family_index{UINT32_MAX};
        uint32_t m_current_frame{0};
        float m_priority = 1.0f;
        VkQueue m_graphics_queue = VK_NULL_HANDLE;
        VkQueue m_present_queue = VK_NULL_HANDLE;
        bool m_layers_supported = false;
    };
}
#endif // RENDERER_VK_H