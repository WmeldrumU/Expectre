#ifndef ENGINE_H
#define ENGINE_H
#include <cstdint>
#include <memory>
#include "RendererVk.h"

namespace Expectre
{

    class Engine
    {
    public:
        Engine();
        ~Engine();
        void start();
        void run();
        void cleanup();
        void draw();
        bool isInitialized();
        void limit_frame_rate(uint32_t desired_fps, uint64_t delta_time);
        bool process_input();
        void add_observer(std::weak_ptr<InputObserver> observer);
        uint32_t frameNumber();

    private:
        bool m_isIntialized{false};
        uint32_t m_frameNumber{0};
        std::vector<std::weak_ptr<InputObserver>> m_observers{};
        std::shared_ptr<Renderer_Vk> m_renderer = nullptr;

    };

}
#endif // ENGINE_H