#ifndef SHARED_H
#define SHARED_H
    namespace shared {
        #define SCREEN_WIDTH 640
        #define SCREEN_HEIGHT 480

        #ifdef NDEBUG
            constexpr bool validation_layers_enabled = false;
        #else
            constexpr bool validation_layers_enabled = true;
        #endif // NDEBUG

    }
#endif // SHARED_H