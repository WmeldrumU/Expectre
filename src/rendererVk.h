#ifndef RENDERER_VK_H
#define RENDERER_VK_H
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <string>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

namespace Expectre
{
    typedef struct _SwapChainBuffers
    {
        VkImage image;
        VkImageView view;
    } SwapChainBuffer;

    struct VsUniform
    {
        // Must start with MVP
        float mvp[4][4];
        float position[12 * 3][4];
        float attr[12 * 3][4];
    };

    class Renderer_Vk
    {

    public:
        Renderer_Vk();
        ~Renderer_Vk();

        bool m_enable_validation_layers{true};
        void cleanup();

    private:
        void create_instance();

        void enumerate_layers();

        void create_surface();

        void query_swap_chain_support();

        void select_physical_device();

        void create_logical_device_and_queues();

        void get_surface_format();

        void create_swap_chain();

        void create_command_buffers();

        void create_vertex_buffer();

        void create_layouts();

        void prepare_depth();

        void prepare_render_pass();
        
        void prepare_pipeline();

        void prepare_present_cmd_pool_and_buffers();

        void create_descriptor_pool_and_sets();

        void create_framebuffers();

        void demo_draw_build_cmd();

        void create_buffers_and_images();

        void create_views();

        void select_surface_formats();

        void get_present_mode();

        void create_semaphors_and_fences();

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
        std::vector<VkDescriptorSet> m_descriptor_sets{};
        VkPipelineCache m_pipeline_cache{};
        VkDescriptorSetLayout m_descriptor_set_layout{};
        std::vector<VkFramebuffer> m_framebuffers{};
        std::vector<const char *> m_layers{"VK_LAYER_KHRONOS_validation"};
        std::vector<VkPhysicalDevice> m_physical_devices;
        std::optional<VkPhysicalDevice> m_chosen_phys_device;
        std::vector<const char *> m_required_instance_extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        std::vector<const char *> m_required_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        std::vector<VkImage> m_swapchain_images{};
        std::vector<SwapChainBuffer> m_swapchain_buffers{};
        std::vector<VkBuffer> m_swapchain_uniform_buffers{};
        std::vector<VkDeviceMemory> m_uniform_memories{};
        VkPhysicalDeviceMemoryProperties m_phys_memory_properties{};
        VkDevice m_device = VK_NULL_HANDLE;
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
        VkCommandPool m_present_cmd_pool = VK_NULL_HANDLE;
        VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;

        struct
        {
            VkFormat format;

            VkImage image;
            VkMemoryAllocateInfo mem_alloc;
            VkDeviceMemory mem;
            VkImageView view;
        } m_depth;

        VkSurfaceFormatKHR m_surface_format{};
        uint32_t m_graphics_queue_family_index{UINT32_MAX};
        uint32_t m_present_queue_family_index{UINT32_MAX};
        float m_priority = 1.0f;
        VkQueue m_graphics_queue;
        VkQueue m_present_queue;
        bool m_layers_supported = false;
    };
}
#endif // RENDERER_VK_H