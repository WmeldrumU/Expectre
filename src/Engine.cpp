#include "Engine.h"
// #include "RendererWgpu.h"
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

  m_input_manager.Subscribe(SDL_EVENT_WINDOW_RESIZED, [&](const SDL_Event e) {
    m_render_context->OnWindowResize(
        glm::uvec2{e.window.data1, e.window.data2});
  });
}

void Engine::run() {

  if (!m_render_context->is_ready()) {
    throw std::runtime_error("renderer could not initialize!");
  }

  static uint64_t last_time = SDL_GetTicks();
  bool quit = false;

  while (!quit) {
    uint64_t current_time = SDL_GetTicks();
    uint64_t delta_time = current_time - last_time;
    last_time = current_time;

    // Update input manager, find which keys are pressed/released this frame
    quit = m_input_manager.Update();
    // Update scene
    m_scene.Update(delta_time, m_input_manager);

    // Render frame
    m_render_context->update_and_render(delta_time, m_scene);

    limit_frame_rate(60, delta_time);
  }

  // SDL cleanup
  SDL_DestroyWindow(m_window);
  SDL_Quit();

  return;
}

void Engine::limit_frame_rate(uint32_t desired_fps, uint64_t delta_time) {
  auto desired_frame_time =
      1000 / desired_fps; // Milliseconds per frame for desired FPS

  if (delta_time < desired_frame_time) {
    auto sleepTime = std::chrono::milliseconds(desired_frame_time - delta_time);
    std::this_thread::sleep_for(sleepTime);
  }
}

} // namespace Expectre