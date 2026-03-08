#ifndef EXPECTRE_OBSERVER_H
#define EXPECTRE_OBSERVER_H
#include <SDL3/SDL.h>

namespace Expectre
{

    class InputObserver
    {
    public:
        virtual ~InputObserver() = default;
        virtual void on_input_event(const SDL_Event &event) = 0;
    };
}
#endif
