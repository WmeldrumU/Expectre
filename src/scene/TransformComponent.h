#ifndef SCENE_TRANSFORM_COMPONENT
#define SCENE_TRANSFORM_COMPONENT

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Component.h"
namespace Expectre {

// Transform component
// Handles the position, rotation, and scale of an entity in 3D space
class TransformComponent : public Component {
public:
  void set_translation(const glm::vec3 &pos) {
    m_position = pos;
    m_transform_dirty = true;
  }

  void set_rotation(const glm::quat &rot) {
    m_rotation = rot;
    m_transform_dirty = true;
  }

  void set_scale(const glm::vec3 &scale) {
    m_scale = scale;
    m_transform_dirty = true;
  }

  const glm::vec3 &get_position() const { return m_position; }
  const glm::quat &get_rotation() const { return m_rotation; }
  const glm::vec3 &get_scale() const { return m_scale; }

  glm::mat4 get_transform_matrix() const {
    if (m_transform_dirty) {
      // Calculate
      glm::mat4 trans_matrix = glm::translate(glm::mat4(1.0f), m_position);
      glm::mat4 rot_matrix = glm::mat4_cast(m_rotation);
      glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), m_scale);
      m_transform_matrix = trans_matrix * rot_matrix * scale_matrix;
      m_transform_dirty = false;
    }
    return m_transform_matrix;
  }

private:
  glm::vec3 m_position = glm::vec3(0.0f);
  glm::quat m_rotation =
      glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
  glm::vec3 m_scale = glm::vec3(1.0f);

  // Cached transform matrix
  mutable glm::mat4 m_transform_matrix = glm::mat4(1.0f);
  mutable bool m_transform_dirty = true;
};

} // namespace Expectre
#endif // SCENE_TRANSFORM_COMPONENT