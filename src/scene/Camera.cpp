#include "scene/Camera.h"
#include <glm/glm.hpp>

namespace Expectre {
Camera::Camera(SceneObject *parent, std::string name)
    : SceneObject(parent, name) {
  m_renderable = false;
}

void Camera::update(uint64_t delta_time, const InputManager &input_manager) {
  // Calculate movement direction based on input
  glm::vec3 movement_vector(0.0f);

  if (input_manager.IsKeyDown(SDLK_W))
    movement_vector += glm::vec3(0.0f, 0.0f, -1.0f);
  if (input_manager.IsKeyDown(SDLK_S))
    movement_vector += glm::vec3(0.0f, 0.0f, 1.0f);
  if (input_manager.IsKeyDown(SDLK_A))
    movement_vector += glm::vec3(-1.0f, 0.0f, 0.0f);
  if (input_manager.IsKeyDown(SDLK_D))
    movement_vector += glm::vec3(1.0f, 0.0f, 0.0f);

  // Normalize direction to prevent faster diagonal movement
  if (glm::length(movement_vector) > 0.0f)
    movement_vector = glm::normalize(movement_vector);

  // Update position with delta time for frame-rate independent movement
  m_position +=
      movement_vector * m_camera_speed * static_cast<float>(delta_time) / 1000.0f;
}

} // namespace Expectre
