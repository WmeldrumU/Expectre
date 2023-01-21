#ifndef RENDERER_VK_H
#define RENDERER_VK_H
#include <vulkan/vulkan.h>

namespace expectre {
    class Renderer_Vk {

    public:
        Renderer_Vk();
        ~Renderer_Vk();

        bool enable_validation_layers{true};

    private:
        void create_instance();
        
        VkInstance m_instance{};
    };
}
#endif // RENDERER_VK_H