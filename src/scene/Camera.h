#ifndef SCENE_CAMERA
#define SCENE_CAMERA

#include "input/InputManager.h"
#include "scene/SceneObject.h"

namespace Expectre {
class Camera : SceneObject {
public:
  Camera() = delete;

  Camera(SceneObject *parent, std::string name);

  // Delete the copy constructor
  Camera(const Camera &other) = delete;
  // Delete the copy assignment operator as well for consistency
  Camera &operator=(const Camera &other) = delete;

  void update(uint64_t delta_time, const InputManager &input_manager);

  glm::vec3 get_position() const { return m_position; }
  glm::vec3 get_forward_dir() const { return m_forward_dir; }

private:
  float m_camera_speed = 3.0f;
  glm::vec3 m_position = {0.0f, 1.0f, 2.0f};
  glm::vec3 m_forward_dir = {0.0f, 0.0f, -1.0f};
};
} // namespace Expectre
#endif // SCENE_CAMERA