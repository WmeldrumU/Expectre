#ifndef ENGINE_H
#define ENGINE_H
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "RenderCommand.h"
#include "RenderContextVk.h"
#include "RingBuffer.h"
#include "input/InputManager.h"
#include "observer.h"
#include "scene/Scene.h"

#ifdef NDEBUG
/**
 * @brief Compile-time constant set to true if NDEBUG is defined (Release
 * build).
 */
constexpr bool _is_debug_build = false;
#else
/**
 * @brief Compile-time constant set to true if NDEBUG is not defined (Debug
 * build).
 */
constexpr bool _is_debug_build = true;
#endif

namespace Expectre {

class Engine {
public:
  Engine();
  void start();
  void run();
  void cleanup();
  void draw();
  bool isInitialized();
  void limit_frame_rate(uint32_t desired_fps, uint64_t delta_time);
  bool process_input();
  void create_surface();
  uint32_t frameNumber();
  bool is_running();

private:
  // static int SDLCALL scene_thread_func(void *ptr);
  static int SDLCALL static_render_thread_entry(void *ptr);
  void run_render_thread();

  bool m_isIntialized{false};
  uint32_t m_frameNumber{0};
  SDL_Window *m_window{};
  std::unique_ptr<RenderContextVk> m_render_context = nullptr;
  InputManager m_input_manager;
  Scene m_scene;
  RingBuffer<RenderCommand, 4096> render_commands;

  // double buffered render commands, allows for scene thread to write
  // commands to one buffer while the render thread reads from another
  std::array<RenderCommands, 2> m_render_command_buffers;

  SDL_AtomicInt m_engine_running;
  SDL_Thread *m_render_thread;
  // SDL_Thread *m_scene_thread;
  SDL_Semaphore *m_render_commands_ready;
  SDL_Semaphore *m_render_command_buffer_available;
};

} // namespace Expectre
#endif // ENGINE_H