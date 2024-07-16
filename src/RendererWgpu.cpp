#include "RendererWgpu.h"
#include "spdlog/spdlog.h"
#include "../lib/sdl2webgpu/sdl2webgpu.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

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
    WGPUDeviceLostCallbackNew deviceLostCallback =
        [](WGPUDevice const *device, WGPUDeviceLostReason reason,
           char const *message, void *userdata)
    {
      spdlog::error("Device lost: {}", message);
    };

    WGPUDeviceLostCallbackInfo deviceLostCallbackInfo{};
    deviceLostCallbackInfo.nextInChain = nullptr;
    deviceLostCallbackInfo.callback = deviceLostCallback;
    deviceLostCallbackInfo.userdata = nullptr;
    // TODO: WHAT IS THIS?
    deviceLostCallbackInfo.mode = WGPUCallbackMode::WGPUCallbackMode_AllowSpontaneous;

    WGPUDeviceDescriptor device_desc{};
    device_desc.label = "My Device";
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredLimits = nullptr;
    device_desc.defaultQueue.nextInChain = nullptr;
    device_desc.defaultQueue.label = "The default queue";
    device_desc.deviceLostCallbackInfo = deviceLostCallbackInfo;
    m_device = requestDeviceSync(m_adapter, &device_desc);
    wgpuAdapterRelease(m_adapter);
    wgpuDeviceSetUncapturedErrorCallback(m_device, [](WGPUErrorType type, char const *message, void *userdata)
                                         { spdlog::error("Uncaptured error: {}", message); }, nullptr);

    WGPUQueue queue = wgpuDeviceGetQueue(m_device);

    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void *userdata1, void *userdata2)
    {
      spdlog::debug("Queued work finished with status: {}", static_cast<uint32_t>(status));
    };

    // Queue work done
    WGPUQueueWorkDoneCallbackInfo2 callback_info{};
    callback_info.mode = WGPUCallbackMode::WGPUCallbackMode_AllowSpontaneous;
    callback_info.callback = onQueueWorkDone;
    callback_info.userdata1 = nullptr;
    callback_info.userdata2 = nullptr;
    callback_info.nextInChain = nullptr;
    // wgpuQueueOnSubmittedWorkDone(queue, callback_info);
    WGPUFuture future = wgpuQueueOnSubmittedWorkDone2(queue, callback_info);
    // wgpuQueueOnSubmittedWorkDoneF(queue, callback_info);

    std::vector<WGPUCommandBuffer> command_buffer(1);

    // Command Encoder
    WGPUCommandEncoderDescriptor command_enc_desc{};
    command_enc_desc.label = "Command Encoder";
    WGPUCommandEncoder command_enc = wgpuDeviceCreateCommandEncoder(m_device, &command_enc_desc);
    wgpuCommandEncoderInsertDebugMarker(command_enc, "Command Encoder");

    WGPUCommandBufferDescriptor command_buffer_desc{};
    command_buffer_desc.label = "Command Buffer";
    command_buffer.push_back(wgpuCommandEncoderFinish(command_enc, &command_buffer_desc));
    wgpuCommandEncoderRelease(command_enc); // release encoder

    spdlog::debug("Submittting command ");
    wgpuQueueSubmit(queue, command_buffer.size(), command_buffer.data());
    spdlog::debug("Command submitted");
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

#ifdef __EMSCRIPTEN__
    while (user_data.request_ended == false)
    {
      emscripten_sleep(1);
    }
#endif
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

#ifdef __EMSCRIPTEN__
    while (user_data.request_ended == false)
    {
      emscripten_sleep(1);
    }
#endif //__EMSCRIPTEN__

    return user_data.device;
  }

  void RendererWgpu::checkAdapterLimits()
  {
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
    // wgpuAdapterRelease(m_adapter);
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