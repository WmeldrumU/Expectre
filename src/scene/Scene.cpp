
#include "Scene.h"

#include <stdexcept>

namespace Expectre
{
    Scene::Scene()
    {
        create_root();
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