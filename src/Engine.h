#ifndef ENGINE_H
#define ENGINE_H
#include <cstdint>
#include <memory>
#include <vector>
#include <SDL3/SDL.h>
	
#include "RenderContextVk.h"
#include "observer.h"

#ifdef NDEBUG
/**
 * @brief Compile-time constant set to true if NDEBUG is defined (Release build).
 */
constexpr bool _is_debug_build = false;
#else
/**
 * @brief Compile-time constant set to true if NDEBUG is not defined (Debug build).
 */
constexpr bool _is_debug_build = true;
#endif

namespace Expectre
{

	class Engine
	{
	public:
		Engine();
		void start();
		void run();
		void cleanup();
		void draw();
		bool isInitialized();
		void limit_frame_rate(uint32_t desired_fps, uint64_t delta_time);
		bool process_input();
		void add_observer(std::weak_ptr<InputObserver> observer);
		void create_surface();
		uint32_t frameNumber();

	private:
		bool m_isIntialized{ false };
		uint32_t m_frameNumber{ 0 };
		std::vector<std::weak_ptr<InputObserver>> m_observers{};
		SDL_Window* m_window{};
		std::unique_ptr<RenderContextVk> m_render_context = nullptr;
	};

}
#endif // ENGINE_H