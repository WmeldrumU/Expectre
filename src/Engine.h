#ifndef ENGINE_H
#define ENGINE_H
#include <cstdint>

namespace expectre {
    
    class Engine {
    public:
        Engine();
        void run();
        void cleanup();
        void draw();
        bool isInitialized();
        uint32_t frameNumber();
    private:
        bool m_isIntialized{false};
        uint32_t m_frameNumber{0};
    };
    
}
#endif // ENGINE_H