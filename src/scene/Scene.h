#ifndef SCENE
#define SCENE

#include "scene/SceneObject.h"
#include "input/InputManager.h"
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
        void Update(float delta_time, const InputManager& input_manager);

    private:

    void create_root();
        std::unique_ptr<SceneObject> m_root;
    };
}
#endif // SCENE