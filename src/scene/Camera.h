#ifndef SCENE_CAMERA
#define SCENE_CAMERA

#include "scene/SceneObject.h"

namespace Expectre
{
    class Camera : SceneObject
    {
        Camera(SceneObject* parent, std::string name);

        // Delete the copy constructor
        Camera(const Camera &other) = delete;
        // Delete the copy assignment operator as well for consistency
        Camera &operator=(const Camera &other) = delete;
        private:
    };
}
#endif // SCENE_CAMERA