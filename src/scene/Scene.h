#ifndef SCENE
#define SCENE

#include "input/InputManager.h"
#include "scene/SceneObject.h"
#include "scene/SceneRoot.h"
#include "scene/Camera.h"
#include "Model.h"

namespace Expectre
{
    class Scene
    {
    public:
        Scene() = delete;
        Scene(std::string scene_name);
        // Delete the copy constructor
        Scene(const Scene &other) = delete;
        // Delete the copy assignment operator as well for consistency
        Scene &operator=(const Scene &other) = delete;
        void Update(uint64_t delta_time, const InputManager &input_manager);
        SceneObject &get_root() { return m_root; }

    private:
        void create_root();

        SceneRoot m_root;
        Camera m_camera;
    };
}
#endif // SCENE