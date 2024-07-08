#include "RendererWgpu.h"
#include "spdlog/spdlog.h"
#include "../lib/sdl2webgpu/sdl2webgpu.h"

namespace Expectre
{
  RendererWgpu::RendererWgpu()
  {

    // Create WebGPU instance
    WGPUInstanceDescriptor desc{};
    desc.nextInChain = nullptr;
    m_instance = wgpuCreateInstance(&desc);

    // Check WebGPU instance

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
      spdlog::error("SDL_Init Error: {}", SDL_GetError());
      throw std::runtime_error("SDL_Init Error");
    }

    if (!m_instance)
    {
      spdlog::error("wgpuCreateInstance Error: {}", SDL_GetError());
      throw std::runtime_error("wgpuCreateInstance Error");
    }

    m_window = SDL_CreateWindow("Hello WebGPU!", SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    WGPUSurface surface = SDL_GetWGPUSurface(m_instance, m_window);
    // Show address instance and surface
    spdlog::debug("RendererWgpu::RendererWgpu(): instance = {:p}", (void *)m_instance);
    spdlog::debug("RendererWgpu::RendererWgpu(): surface = {:p}", (void *)surface);

   
    WGPURequestAdapterOptions options{};
    // TODO: The second arg will need to change as we take in options for the adapter
    m_adapter = requestAdapterSync(m_instance, &options);
    checkAdapterLimits();

    WGPUDeviceDescriptor device_desc{};
    m_device = requestDeviceSync(m_adapter, &device_desc);
    spdlog::info("testing");
  }


  WGPUAdapter RendererWgpu::requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const *options)
  {
    struct UserData
    {
      WGPUAdapter adapter = nullptr;
      bool request_ended = false;
    };
    UserData user_data;

    auto callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const *message, void *userdata)
    {
      UserData &user_data = *reinterpret_cast<UserData *>(userdata);
      if (status == WGPURequestAdapterStatus_Success)
      {
        user_data.adapter = adapter;
      }
      else
      {
        spdlog::error("Failed to request adapter: {}", message);
        std::runtime_error("Failed to request adapter");
      }
      user_data.request_ended = true;
    };

    wgpuInstanceRequestAdapter(instance, options, callback, static_cast<void *>(&user_data));

    return user_data.adapter;
  }
  WGPUDevice RendererWgpu::requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const *descriptor)
  {
    struct UserData
    {
      WGPUDevice device = nullptr;
      bool request_ended = false;
    };
    UserData user_data;

    auto callback = [](WGPURequestDeviceStatus status, WGPUDevice device, char const *message, void *userdata)
    {
      UserData &user_data = *reinterpret_cast<UserData *>(userdata);
      if (status == WGPURequestDeviceStatus_Success)
      {
        user_data.device = device;
      }
      else
      {
        spdlog::error("Failed to request device: {}", message);
        std::runtime_error("Failed to request device");
      }
      user_data.request_ended = true;
    };

    wgpuAdapterRequestDevice(m_adapter, descriptor, callback, static_cast<void *>(&user_data));

    return user_data.device;
  }

  void RendererWgpu::checkAdapterLimits() {
    bool success = wgpuAdapterGetLimits(m_adapter, &m_limits) == WGPUStatus_Success;

    // Show adapter limits
    if (success)
    {
      spdlog::info("Adapter limits:");
      spdlog::info(" - maxTextureDimension1D: {}", m_limits.limits.maxTextureDimension1D);
      spdlog::info(" - maxTextureDimension2D: {}", m_limits.limits.maxTextureDimension2D);
      spdlog::info(" - maxTextureDimension3D: {}", m_limits.limits.maxTextureDimension3D);
      spdlog::info(" - maxTextureArrayLayers: {}", m_limits.limits.maxTextureArrayLayers);
    }
  }

  RendererWgpu::~RendererWgpu()
  {
    spdlog::debug("RendererWgpu::~RendererWgpu()");
    wgpuDeviceRelease(m_device);
    wgpuAdapterRelease(m_adapter);
    wgpuInstanceRelease(m_instance);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
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