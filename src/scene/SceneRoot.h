#ifndef EXPECTRE_SCENEROOT_H
#define EXPECTRE_SCENEROOT_H
#include "SceneObject.h"
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
      m_world_transform = glm::mat3x4{};
      m_relative_transform = glm::mat3x4{};
      m_name = name;
    }

    glm::mat3x4 get_world_transform() override
    {
      return glm::mat3x4{};
    }

    glm::mat3x4 get_relative_transform() override
    {
      return glm::mat3x4{};
    }
  };
} // namespace Expectre
#endif // EXPECTRE_SCENEROOT_H