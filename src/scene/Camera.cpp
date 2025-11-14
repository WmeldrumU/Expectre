#include "scene/Camera.h"

namespace Expectre
{
        Camera::Camera(SceneObject* parent, std::string name = "") : SceneObject(parent, name){
            m_renderable = false;

        }
}
