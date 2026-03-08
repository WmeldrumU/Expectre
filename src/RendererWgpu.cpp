#include "RendererWgpu.h"
#include "spdlog/spdlog.h"
#include "../lib/sdl2webgpu/sdl2webgpu.h"

namespace Expectre
{
  RendererWgpu::RendererWgpu()
  {

    WGPUInstanceDescriptor desc{};
    desc.nextInChain = nullptr;
    m_instance = wgpuCreateInstance(&desc);

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
      spdlog::error("SDL_Init Error: {}", SDL_GetError());
      throw std::runtime_error("SDL_Init Error");
    }

    m_window = SDL_CreateWindow("Hello WebGPU!", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    WGPUSurface surface = SDL_GetWGPUSurface(m_instance, m_window);
    spdlog::debug("RendererWgpu::RendererWgpu(): surface = {:p}",(void*)surface);
    
  }

  RendererWgpu::~RendererWgpu()
  {
    spdlog::debug("RendererWgpu::~RendererWgpu()");
  }

  void RendererWgpu::cleanup()
  {
    spdlog::debug("RendererWgpu::cleanup()");
  }

  bool RendererWgpu::isReady()
  {
    spdlog::debug("RendererWgpu::isReady()");
    return true;
  }

  void RendererWgpu::draw_frame()
  {
    spdlog::debug("RendererWgpu::draw_frame()");
  }
}