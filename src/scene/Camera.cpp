#include "scene/Camera.h"
#include <glm/glm.hpp>

namespace Expectre {
Camera::Camera(SceneObject *parent, std::string name)
    : SceneObject(parent, name) {
  m_renderable = false;
}

void Camera::update(uint64_t delta_time, const InputManager &input_manager) {
  // Use the camera's forward direction and calculate right vector
  glm::vec3 forward = glm::normalize(m_forward_dir);
  glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::normalize(glm::cross(forward, world_up));
  glm::vec3 up = glm::normalize(glm::cross(right, forward));

  // Calculate movement direction based on camera's local axes
  glm::vec3 movement_vector(0.0f);

  if (input_manager.IsKeyDown(SDLK_W))
    movement_vector += forward; // Move forward
  if (input_manager.IsKeyDown(SDLK_S))
    movement_vector -= forward; // Move backward
  if (input_manager.IsKeyDown(SDLK_A))
    movement_vector -= right; // Move left
  if (input_manager.IsKeyDown(SDLK_D))
    movement_vector += right; // Move right
  if (input_manager.IsKeyDown(SDLK_Q))
    movement_vector -= up;
  if (input_manager.IsKeyDown(SDLK_E))
    movement_vector += up;

  // Normalize direction to prevent faster diagonal movement
  if (glm::length(movement_vector) > 0.0f)
    movement_vector = glm::normalize(movement_vector);

  // Update position with delta time for frame-rate independent movement
  m_position += movement_vector * m_camera_speed *
                static_cast<float>(delta_time) / 1000.0f;
}

} // namespace Expectre
