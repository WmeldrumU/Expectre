#ifndef SCENE_ENTITY
#define SCENE_ENTITY

#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "scene/Component.h"

namespace Expectre {

using EntityId = uint32_t;
static constexpr EntityId kInvalidEntity = std::numeric_limits<EntityId>::max();
class Entity {
public:
  Entity() = default;
  Entity(const std::string &name, EntityId parent = kInvalidEntity)
      : m_name(name), m_parent(parent) {}
  template <typename T, typename... Args> T *add_component(Args &&...args) {
    static_assert(std::is_base_of<Component, T>::value,
                  "T must derive from Component>");

    size_t type_id = Component::get_type_id<T>();

    // Check if component of this type already exists
    auto it = m_component_map.find(type_id);
    if (it != m_component_map.end()) {
      return static_cast<T *>(it->second);
    }

    // Create new component
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T *p_component = component.get();
    m_component_map[type_id] = p_component;
    m_components.push_back(std::move(component));
    return p_component;
  }

  // Add move constructor and move assignment
  Entity(Entity &&other) noexcept = default;
  Entity &operator=(Entity &&other) noexcept = default;

  template <typename T> T *get_component() const {

    size_t type_id = Component::get_type_id<T>();

    // check if exists

    auto it = m_component_map.find(type_id);
    if (it != m_component_map.end()) {
      return static_cast<T *>(it->second);
    }

    return nullptr;
  }

  template <typename T> bool remove_component() {
    size_t type_id = Component::get_type_id<T>();
    auto it = m_component_map.find(type_id);
    if (it != m_component_map.end()) {
      Component *p_component = it->second;
      m_component_map.erase(it);

      for (auto comp_it = m_components.begin(); comp_it != m_components.end();
           ++comp_it) {
        if (comp_it->get() == p_component) {
          m_components.erase(comp_it);
          return true;
        }
      }
    }

    return false;
  }

  void set_parent(EntityId parent) { m_parent = parent; }

private:
  std::string m_name;
  EntityId m_parent;
  std::vector<std::unique_ptr<Component>> m_components;
  std::unordered_map<size_t, Component *> m_component_map;
};
} // namespace Expectre
#endif // SCENE_ENTITY