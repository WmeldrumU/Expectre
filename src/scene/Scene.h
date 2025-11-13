#ifndef SCENE
#define SCENE

#include "SceneNode.h"


namespace Expectre
{
    class Scene
    {

        Scene();
        // Delete the copy constructor
        Scene(const Scene &other) = delete;
        // Delete the copy assignment operator as well for consistency
        Scene &operator=(const Scene &other) = delete;

        SceneObject m_root{};
    };
}
#endif // SCENE