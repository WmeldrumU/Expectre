#ifndef ENGINE_H
#define ENGINE_H
#include <cstdint>
#include <memory>
#include "rendererVk.h"

namespace Expectre {
    
    class Engine {
    public:
        Engine();
        void start();
        void run();
        void cleanup();
        void draw();
        bool isInitialized();
        uint32_t frameNumber();
    private:
        bool m_isIntialized{false};
        uint32_t m_frameNumber{0};
        std::shared_ptr<Renderer_Vk> m_renderer = nullptr;
    };
    
}
#endif // ENGINE_H