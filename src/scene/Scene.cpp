
#include "scene/Scene.h"

#include <stdexcept>

namespace Expectre
{
    Scene::Scene()
    {
        create_root();
    }
    void Scene::Update(float delta_time, const InputManager &input_manager)
    {
        // Update scene logic here, e.g., traverse scene graph, update animations, etc.
        // This is a placeholder implementation.
        (void)delta_time;    // suppress unused parameter warning
        (void)input_manager; // suppress unused parameter warning
    }

    void Scene::create_root()
    {
        if (m_root != nullptr)
        {
            throw std::logic_error("Root already exists");
        }
        m_root = std::make_unique<SceneObject>(nullptr, "root");
    }

}