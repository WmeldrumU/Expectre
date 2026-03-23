#ifndef SCENE_CAMERA
#define SCENE_CAMERA

#include "Component.h"
#include "input/InputManager.h"

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>

namespace Expectre {
class CameraComponent : Component {
public:
  void update(uint64_t delta_time, const InputManager &input_manager) {
    // Use the camera's forward direction and calculate right vector
    glm::vec3 forward = glm::normalize(this->m_forward_dir);
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
  glm::vec3 get_position() const { return m_position; }
  glm::vec3 get_forward_dir() const { return m_forward_dir; }

private:
  float m_camera_speed = 3.0f;
  glm::vec3 m_position = {2.0f, 1.0f, 8.0f};
  glm::vec3 m_forward_dir = {0.0f, 0.0f, -1.0f};

}; // class CameraComponent
} // namespace Expectre
#endif // SCENE_CAMERA