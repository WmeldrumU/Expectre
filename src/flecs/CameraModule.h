#ifndef FLECS_CAMERA_MODULE
#define FLECS_CAMERA_MODULE

#include <flecs.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Expectre {

struct Camera {
  float speed = 3.0f;
  // glm::vec3 position = {2.0f, 1.0f, 8.0f};
  // glm::vec3 forward_dir = {0.0f, 0.0f, -1.0f};
};
struct CameraModule {
  CameraModule(flecs::world &world);

private:
  void register_camera_system(flecs::world &world);
};
} // namespace Expectre

#endif // FLECS_CAMERA_MODULE