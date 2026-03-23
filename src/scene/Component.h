#ifndef SCENE_COMPONENT
#define SCENE_COMPONENT

namespace Expectre {

// Component type ID system
class ComponentTypeIDSystem {
private:
  inline static size_t next_type_id = 0;

public:
  template <typename T> static size_t GetTypeID() {
    static size_t type_id = next_type_id++;
    return type_id;
  }
};

class Component {
public:
  virtual ~Component() = default;

  template <typename T> static size_t get_type_id() {
    return ComponentTypeIDSystem::GetTypeID<T>();
  }
};
} // namespace Expectre

#endif // SCENE_COMPONENT