#include "Engine.h"
//#include "RendererWgpu.h"
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#define USE_WEBGPU
namespace Expectre
{
    Engine::Engine()
    {
    }

    void Engine::run()
    {
        #if defined(USE_WEBGPU)
        // spdlog::debug("Using WebGPU");
        //         m_renderer =
        //             std::make_shared<RendererWgpu>();
        #elif defined(USE_DIRECTX)
                m_renderer = std::make_shared<Renderer_Dx>();
        #else
                m_renderer =
                    std::make_shared<Renderer_Vk>();
        #endif
        // if (!m_renderer->isReady())
        // {
        //     throw std::runtime_error("renderer could not initialize!");
        // }
        bool quit = false;
        SDL_Event sdl_event;

        while (!quit)
        {
            // Handle events on queue
            while (SDL_PollEvent(&sdl_event) != 0)
            {
                // User requests quit
                if (sdl_event.type == SDL_QUIT)
                {
                    quit = true;
                }
            }

            // Render frame
            m_renderer->draw_frame();

            limit_frame_rate(60);
        }
    }

    void Engine::limit_frame_rate(uint32_t desiredFPS)
    {
        static auto lastTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto frameTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count();
        auto desiredFrameTime = 1000 / desiredFPS; // Milliseconds per frame for desired FPS

        if (frameTime < desiredFrameTime)
        {
            auto sleepTime = std::chrono::milliseconds(desiredFrameTime - frameTime);
            std::this_thread::sleep_for(sleepTime);
        }

        lastTime = std::chrono::high_resolution_clock::now();
    }

    void Engine::cleanup()
    {
    }

    void Engine::draw()
    {
    }
}