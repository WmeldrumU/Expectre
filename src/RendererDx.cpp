// #include "RendererDx.h"

// namespace Expectre
// {
//   Renderer_Dx::Renderer_Dx()
//   {
//     if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
//     {
//       spdlog::error("SDL_Init Error: {}", SDL_GetError());
//       throw std::runtime_error("SDL_Init Error");
//     }

//     SDL_Window *window = SDL_CreateWindow("Hello DX12!", SDL_WINDOWPOS_CENTERED,
//                                           SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
//     spdlog::debug("Renderer_Dx::Renderer_Dx()");
//   }

//   Renderer_Dx::~Renderer_Dx()
//   {
//     spdlog::debug("Renderer_Dx::~Renderer_Dx()");
//   }

//   void Renderer_Dx::cleanup()
//   {
//     spdlog::debug("Renderer_Dx::cleanup()");
//   }

//   bool Renderer_Dx::isReady()
//   {
//     spdlog::debug("Renderer_Dx::isReady()");
//     return true;
//   }

//   void Renderer_Dx::draw_frame()
//   {
//     spdlog::debug("Renderer_Dx::draw_frame()");
//   }
// } // namespace Expectre