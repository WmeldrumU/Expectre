#ifndef SCENE
#define SCENE

#include "SceneObject.h"

namespace Expectre
{
    class Scene
    {
    public:
        Scene();
        // Delete the copy constructor
        Scene(const Scene &other) = delete;
        // Delete the copy assignment operator as well for consistency
        Scene &operator=(const Scene &other) = delete;


    private:

    void create_root();
        std::unique_ptr<SceneObject> m_root;
    };
}
#endif // SCENE