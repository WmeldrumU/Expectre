#ifndef ENGINE_H
#define ENGINE_H
#include <cstdint>
#include "rendererVk.h"

namespace Expectre {
    
    class Engine {
    public:
        Engine() = delete;
        Engine(Renderer_Vk* renderer);
        void start();
        void run();
        void cleanup();
        void draw();
        bool isInitialized();
        uint32_t frameNumber();
    private:
        bool m_isIntialized{false};
        uint32_t m_frameNumber{0};
        Renderer_Vk* m_renderer;
    };
    
}
#endif // ENGINE_H