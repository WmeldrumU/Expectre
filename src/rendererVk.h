#ifndef RENDERER_VK_H
#define RENDERER_VK_H
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

namespace Expectre
{
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

        void create_swap_chain();

        void select_physical_device();

        void create_logical_device_and_queues();

        void create_buffers_and_images();

        void create_views();
        uint32_t choose_heap_from_flags(const VkMemoryRequirements &memoryRequirements, 
        VkMemoryPropertyFlags requiredFlags, 
        VkMemoryPropertyFlags prefferedFlags);

        SDL_Window *m_window{};
        VkInstance m_instance{};
        VkSurfaceKHR m_surface{};
        VkBufferView m_buffer_view{};
        VkImageView m_image_view{};
        std::vector<const char *> m_layers{"VK_LAYER_KHRONOS_validation"};
        std::vector<VkPhysicalDevice> m_physical_devices;
        std::optional<VkPhysicalDevice> m_chosen_phys_device;
        std::vector<const char *> m_supported_extensions = {VK_KHR_SURFACE_EXTENSION_NAME};
        VkDevice m_device = VK_NULL_HANDLE;
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        uint32_t graphics_queue_family_index{UINT32_MAX};
        uint32_t present_queue_family_index{UINT32_MAX};
        float m_priority = 1.0f;
        VkQueue m_graphics_queue;
        VkQueue m_present_queue;
        bool m_layers_supported = false;
    };
}
#endif // RENDERER_VK_H