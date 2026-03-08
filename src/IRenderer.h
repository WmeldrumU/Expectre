#ifndef IRENDERER_H
#define IRENDERER_H

#include <memory>
#include <SDL3/SDL.h>

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void draw_frame() = 0;
    // Add other common methods here
private:

protected:
};

#endif // IRENDERER_H