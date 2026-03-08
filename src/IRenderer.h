#ifndef IRENDERER_H
#define IRENDERER_H

#include <memory>
#include <SDL2/SDL.h>

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void draw_frame() = 0;
    // Add other common methods here
private:

protected:
    SDL_Window* m_window;
};

#endif // IRENDERER_H