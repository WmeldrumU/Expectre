#include "Engine.h"
#include "AppTime.h"
#include "RenderContextVk.h"
#include "scene/Scene.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

namespace Expectre {
Engine::Engine() : m_scene("Main Scene") {

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    throw std::runtime_error("failed to initialize SDL!");
  }

  m_window =
      SDL_CreateWindow("Expectre", STARTING_RESOLUTION_X, STARTING_RESOLUTION_Y,
                       SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  if (!m_window) {
    SDL_Log("Unable to initialize application window!: %s", SDL_GetError());
    throw std::runtime_error("Unable to initialize application window!");
  }

#if defined(USE_WEBGPU)
  spdlog::debug("Using WebGPU");
  // m_render_context = std::make_shared<RenderContextWgpu>();
#elif defined(USE_DIRECTX)
  // m_render_context = std::make_unique<RenderContextDx>(m_window);
#else
  m_render_context =
      std::make_unique<RenderContextVk>(m_window, m_input_manager);
#endif

  m_render_commands_ready = SDL_CreateSemaphore(0);
  // Initialize to 2 since both commands buffers are available to the scene
  // thread at start
  m_render_command_buffer_available = SDL_CreateSemaphore(2);
  if (!m_render_commands_ready || !m_render_command_buffer_available) {
    throw std::runtime_error(
        std::string("Failed to create render semaphores: ") + SDL_GetError());
  }
}

void Engine::run() {

  if (!m_render_context->is_ready()) {
    throw std::runtime_error("renderer could not initialize!");
  }

  m_render_thread = SDL_CreateThread(
      static_render_thread_entry, "Render Thread", static_cast<void *>(this));
  if (!m_render_thread) {
    throw std::runtime_error(std::string("Failed to create render thread: ") +
                             SDL_GetError());
  }

  // Render commands buffer write index
  size_t write_index = 0;
  uint64_t last_time = SDL_GetTicks();

  while (is_running()) {

    // Shifts "current" key state to "previous" key state, get mouse state
    m_input_manager.update();

    // Drain event queue
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      process_platform_event(event);
    }

    // Wait until a command buffer is avaible to write
    SDL_WaitSemaphore(m_render_command_buffer_available);

    if (!is_running()) {
      break;
    }

    const uint64_t current_time = SDL_GetTicks();
    uint64_t delta_time = current_time - last_time;
    last_time = current_time;

    RenderCommands &commands = m_render_command_buffers[write_index];

    // The render thread should have cleared this buffer after consuming it
    assert(commands.empty());

    // Update scene
    m_scene.Update(delta_time, m_input_manager, commands);

    // Publish this completed buffer
    SDL_SignalSemaphore(m_render_commands_ready);
    // Producer (scene thread) fills in alternating order
    write_index = 1 - write_index;
    // limit_frame_rate(60, delta_time);
  }

  SDL_SetAtomicInt(&m_engine_running, 0);

  // Wake the render thread if it is blocked
  SDL_SignalSemaphore(m_render_commands_ready);

  // Wait for worker threads to finish their current loops
  // int scene_thread_return_value;
  int render_thread_return_value;
  SDL_WaitThread(m_render_thread, &render_thread_return_value);

  // SDL cleanup
  SDL_DestroySemaphore(m_render_commands_ready);
  SDL_DestroySemaphore(m_render_command_buffer_available);
  SDL_DestroyWindow(m_window);
  SDL_Quit();

  return;
}

void Engine::process_platform_event(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_QUIT: {
    SDL_SetAtomicInt(&m_engine_running, 0);
    break;
  }
  // handled by inputmanager
  case SDL_EVENT_KEY_DOWN:
  case SDL_EVENT_KEY_UP: {
    m_input_manager.process_key_event(event);
    break;
  }
  // handled render context
  case SDL_EVENT_WINDOW_RESIZED: {
    m_window_state.dims = glm::uvec2{event.window.data1, event.window.data2};
    m_window_state.trigger_resize_pending();
    break;
  }
  case SDL_EVENT_WINDOW_FOCUS_LOST: {
    // m_input_manager.ResetAllStates(); // Avoids the sticky-key bug on Alt-Tab
    break;
  }
  }
}

// void Engine::limit_frame_rate(uint32_t desired_fps, uint64_t delta_time) {
//   auto desired_frame_time =
//       1000 / desired_fps; // Milliseconds per frame for desired FPS

//   if (delta_time < desired_frame_time) {
//     auto sleepTime = std::chrono::milliseconds(desired_frame_time -
//     delta_time); std::this_thread::sleep_for(sleepTime);
//   }
// }

bool Engine::is_running() { return SDL_GetAtomicInt(&m_engine_running) != 0; }

void Engine::run_render_thread() {

  // Render command buffer read index
  size_t read_index = 0;
  uint64_t last_time = SDL_GetTicks();

  bool window_resize_pending = true;

  while (is_running()) {
    // wait until we have render commands to injest
    SDL_WaitSemaphore(m_render_commands_ready);

    const uint64_t current_time = SDL_GetTicks();
    const uint64_t delta_time = current_time - last_time;
    last_time = current_time;

    // This check prevents deadlock where engine is shut down
    // while render thread is waiting.
    // "Was this thread awoken to be shut down?"
    if (!is_running()) {
      break;
    }

    // check if main thread triggered a window resize
    if (m_window_state.should_resize()) {
      m_render_context->OnWindowResize(m_window_state.dims);
      m_window_state.clear_resize_pending();
    }

    RenderCommands &commands = m_render_command_buffers[read_index];
    // Render frame
    m_render_context->update_and_render(delta_time, commands);
    // clear command buffer
    commands.clear();

    // signal a buffer is available for reuse
    SDL_SignalSemaphore(m_render_command_buffer_available);

    // Render thread consumes buffers in the same order the scene thread
    // produces
    read_index = 1 - read_index;
  }
}

// int SDLCALL Engine::scene_thread_func(void *ptr) { return 0; }
int SDLCALL Engine::static_render_thread_entry(void *ptr) {
  Engine *engine = static_cast<Engine *>(ptr);
  engine->run_render_thread();
  return 0;
}

} // namespace Expectre