#include <SDL2/SDL.h>
#include <webgpu/webgpu.h>
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
    WGPUInstance m_instance;
  };
}
