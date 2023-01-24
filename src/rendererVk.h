#ifndef RENDERER_VK_H
#define RENDERER_VK_H
#include <vulkan/vulkan.h>
#include <vector>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

namespace expectre {
    class Renderer_Vk {

    public:
        Renderer_Vk();
        ~Renderer_Vk();

        bool m_enable_validation_layers{true};

    private:
        void create_instance();

        void create_debug();
        
        void create_surface();

        void select_physical_device();

        void select_queue_family();
        void create_logical_device_and_queues();

        SDL_Window* m_window{};
        VkInstance m_instance{};
        VkSurfaceKHR m_surface{};
        std::vector<const char*> m_validation_layers;
        std::vector<VkPhysicalDevice> m_physical_devices;
        VkDevice m_device;
        uint32_t graphics_queue_family_index{UINT32_MAX};
        uint32_t present_queue_family_index{UINT32_MAX};
        VkQueue graphics_queue;
        VkQueue present_queue;
    };
}
#endif // RENDERER_VK_H