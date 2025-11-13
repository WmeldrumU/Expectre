#ifndef SCENE_OBJECT
#define SCENE_OBJECT
#include <vector>

namespace Expectre
{
    class SceneObject
    {

        SceneObject();
        // Delete the copy constructor
        SceneObject(const SceneObject &other) = delete;
        // Delete the copy assignment operator as well for consistency
        SceneObject &operator=(const SceneObject &other) = delete;
        
        SceneObject& m_parent;
        std::vector<SceneObject&> children;
    };
}
#endif // SCENE_OBJECT