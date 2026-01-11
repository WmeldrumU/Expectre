#include "scene/Camera.h"

namespace Expectre {
Camera::Camera(SceneObject *parent, std::string name = "")
    : SceneObject(parent, name) {
  m_renderable = false;
}

void Camera::update(uint64_t delta_time, const InputManager &input_manager) {}

} // namespace Expectre
