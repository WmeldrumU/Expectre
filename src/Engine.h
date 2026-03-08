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
        bool isInitialized() { return m_isIntialized; }
        uint32_t frameNumber { return m_frameNumber; }
    private:
        bool m_isIntialized{false};
        uint32_t m_frameNumber{0};
    };
    
}
#endif // ENGINE_H