#ifndef SCENE_TRANSFORM_MODULE
#define SCENE_TRANSFORM_MODULE

#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Expectre {

struct Transform {
  glm::vec3 translation = glm::vec3(0.0f);
  glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
  glm::vec3 scale = glm::vec3(1.0f);
};

struct TransformModule {
  TransformModule(flecs::world &world);
};

struct WorldMatrix {
  glm::mat4 mat = glm::mat4(1.0f);
};

} // namespace Expectre
#endif // SCENE_TRANSFORM_MODULE