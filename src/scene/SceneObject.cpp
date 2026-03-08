#include "scene/SceneObject.h"
#include <stdexcept>
#include <string>

namespace Expectre {

SceneObject::SceneObject(SceneObject *parent, std::string name = "")
    : m_parent{parent} {
  if (parent == nullptr) {
    throw std::runtime_error("parent cannot be \
                                null unless the current object is scene root.");
  }
  m_world_transform = parent->get_world_transform();
  m_relative_transform = glm::mat4x3{};

  if (name == "") {
    m_name = parent->get_name() + "/" +
             std::to_string(parent->get_children().size());
  } else {
    m_name = parent->get_name() + "/" + name;
  }
}

void SceneObject::set_relative_transform(const glm::mat4x3 &transform) {}

void SceneObject::set_world_transform(const glm::mat4x3 &transform) {
  m_world_transform = transform;
}

void SceneObject::add_child(std::shared_ptr<SceneObject> child) {
  auto child_relative_trf = child->get_relative_transform();
  auto parent_world_trf = get_world_transform();
  auto child_world_trf =
      Expectre::MathUtils::apply_trf(parent_world_trf, child_relative_trf);
  child->set_world_transform(child_world_trf);

  m_children.push_back(child);
}

} // namespace Expectre
