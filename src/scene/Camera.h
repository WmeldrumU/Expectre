#ifndef SCENE_CAMERA
#define SCENE_CAMERA

#include "scene/SceneNode.h"

namespace Expectre
{
    class Camera
    {
        Camera();

        // Delete the copy constructor
        Object(const SceneNode &other) = delete;
        // Delete the copy assignment operator as well for consistency
        SceneNode &operator=(const SceneNode &other) = delete;
    };
}
#endif // SCENE_CAMERA