#ifndef EXPECTRE_SCENEROOT_H
#define EXPECTRE_SCENEROOT_H
#include "scene/SceneObject.h"
#include <string>
#include <glm/mat3x4.hpp>

namespace Expectre
{
  class SceneRoot : public SceneObject
  {
  public:
    SceneRoot(std::string name = "SceneRoot") : SceneObject()
    {
      m_parent = nullptr;
      m_world_transform = glm::mat4x3{};
      m_relative_transform = glm::mat4x3{};
      m_name = name;
    }

    glm::mat4x3 get_world_transform() override
    {
      //todo - implement
      return m_world_transform;
    }

    glm::mat4x3 get_relative_transform() override
    {
      return m_relative_transform;
    }
  };
} // namespace Expectre
#endif // EXPECTRE_SCENEROOT_H