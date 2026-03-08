#ifndef SCENE
#define SCENE

#include "Model.h"
#include "input/InputManager.h"
#include "scene/Camera.h"
#include "scene/SceneObject.h"
#include "scene/SceneRoot.h"

namespace Expectre {
class Scene {
public:
  Scene() = delete;
  Scene(std::string scene_name);
  // Delete the copy constructor
  Scene(const Scene &other) = delete;
  // Delete the copy assignment operator as well for consistency
  Scene &operator=(const Scene &other) = delete;
  void Update(uint64_t delta_time, const InputManager &input_manager);
  SceneObject &get_root() { return m_root; }
  Camera &get_camera() { return m_camera; }

private:
  void create_root();

  SceneRoot m_root;
  Camera m_camera;
};
} // namespace Expectre
#endif // SCENE