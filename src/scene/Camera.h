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

private:
};
} // namespace Expectre
#endif // SCENE_CAMERA