#include "Engine.h"
#include "rendererVk.h"

namespace Expectre
{
    Engine::Engine()
    {
    }

    void Engine::run()
    {
        std::shared_ptr<Renderer_Vk> m_renderer =
            std::make_shared<Renderer_Vk>();
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

            // Present the frame
            // ...
        }
    }

    void Engine::cleanup()
    {
    }

    void Engine::draw()
    {
    }
}