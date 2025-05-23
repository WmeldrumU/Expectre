#include "Engine.h"
// #include "RendererWgpu.h"
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
namespace Expectre
{
	Engine::Engine()
	{
#if defined(USE_WEBGPU)
		spdlog::debug("Using WebGPU");
		m_renderer =
			std::make_shared<RendererWgpu>();
#elif defined(USE_DIRECTX)
		m_renderer = std::make_shared<Renderer_Dx>();
#else
		m_renderer =
			std::make_shared<Renderer_Vk>();
#endif
		// make a weak ptr for observer input notifications
		std::weak_ptr<InputObserver> input_observer(m_renderer);
		add_observer(input_observer);
	}

	void Engine::run()
	{

		if (!m_renderer->isReady())
		{
			throw std::runtime_error("renderer could not initialize!");
		}
		static uint64_t last_time = SDL_GetTicks();
		bool quit = false;

		while (!quit)
		{
			uint64_t current_time = SDL_GetTicks();
			uint64_t delta_time = current_time - last_time;
			last_time = current_time;

			// Handle events on queue
			quit = process_input();

			// Render frame
			m_renderer->update(delta_time);
			m_renderer->draw_frame();

			limit_frame_rate(60, delta_time);
		}
		return;
	}

	void Engine::limit_frame_rate(uint32_t desired_fps, uint64_t delta_time)
	{
		auto desired_frame_time = 1000 / desired_fps; // Milliseconds per frame for desired FPS

		if (delta_time < desired_frame_time)
		{
			auto sleepTime = std::chrono::milliseconds(desired_frame_time - delta_time);
			std::this_thread::sleep_for(sleepTime);
		}
	}

	void Engine::draw()
	{
	}

	void Engine::add_observer(std::weak_ptr<InputObserver> observer)
	{
		m_observers.push_back(observer);
	}

	bool Engine::process_input()
	{
		bool quit = false;
		SDL_Event event;
		// Handle events on queue
		while (SDL_PollEvent(&event) != 0)
		{
			// User requests quit
			if (event.type == SDL_EVENT_QUIT)
			{
				quit = true;
			}
			// Handle input
			else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
			{
				for (auto& weak_observer : m_observers)
				{
					std::shared_ptr<InputObserver> observer_ptr = weak_observer.lock();
					if (!observer_ptr)
						continue;
					observer_ptr->on_input_event(event);
				}
			}
		}
		return quit;
	}

}