#ifdef USE_WEBGPU


#include <SDL2/SDL.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>
// #include "../lib/sdl2webgpu/sdl2webgpu.h"

#include "IRenderer.h"
namespace Expectre
{
  class RendererWgpu : public IRenderer
  {
  public:
    RendererWgpu();
    ~RendererWgpu();

    bool m_enable_validation_layers{true};
    void cleanup();
    bool isReady();
    void draw_frame();

  private:
    WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const *descriptor);
    WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const *options);

    void checkAdapterLimits();
    
    WGPUInstance m_instance{};
    WGPUAdapter m_adapter{};
    WGPUSupportedLimits m_limits{};
    WGPUDevice m_device{};
  };
}
#endif
