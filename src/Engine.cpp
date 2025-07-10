#include "Engine.h"
// #include "RendererWgpu.h"
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

//#include <NsGui/IntegrationAPI.h>
//#include <NsGui/IRenderer.h>
//#include <NsGui/IView.h>
//#include <NsGui/Grid.h>
//#include <NsGui/Uri.h>
// Renderer macros
#define RESOLUTION_X 1280
#define RESOLUTION_Y 720

#define EXTENT { RESOLUTION_X, RESOLUTION_Y }

namespace Expectre
{
	Engine::Engine()
	{
		//Noesis::SetLogHandler([](const char *, uint32_t, uint32_t level, const char *, const char *msg)
		//					  {
  //     // [TRACE] [DEBUG] [INFO] [WARNING] [ERROR]
  //     const char* prefixes[] = { "T", "D", "I", "W", "E" };
  //     printf("[NOESIS/%s] %s\n", prefixes[level], msg); });

		//Noesis::GUI::Init();

		//using namespace Noesis;

		//Ptr<FrameworkElement> xaml = Noesis::GUI::LoadXaml<FrameworkElement>("Reflections.xaml");
		//Ptr<IView> view = Noesis::GUI::CreateView(xaml);
		//view->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
		//view->SetSize(1024, 768);
		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
		{
			SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
			throw std::runtime_error("failed to initialize SDL!");
		}


		m_window = SDL_CreateWindow("Expectre",
			RESOLUTION_X, RESOLUTION_Y,
			SDL_WINDOW_VULKAN);

		if (!m_window)
		{
			SDL_Log("Unable to initialize application window!: %s", SDL_GetError());
			throw std::runtime_error("Unable to initialize application window!");
		}

#if defined(USE_WEBGPU)
		spdlog::debug("Using WebGPU");
		m_renderer =
			std::make_shared<RendererWgpu>();
#elif defined(USE_DIRECTX)
		m_renderer = std::make_shared<Renderer_Dx>();
#else
		m_renderer =
			std::make_shared<RendererVk>(m_window, RESOLUTION_X, RESOLUTION_Y);
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

			// Handle events on input queue
			quit = process_input();

			// Render frame
			m_renderer->update(delta_time);
			m_renderer->draw_frame();

			limit_frame_rate(60, delta_time);
		}

		// SDL cleanup
		SDL_DestroyWindow(m_window);
		//SDL_Vulkan_UnloadLibrary();
		SDL_Quit();

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