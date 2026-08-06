#include "flecs/TransformModule.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Expectre {

void TransformModule::RegisterTransformDirtyObserver(flecs::world &world) {
  // world.defer_begin();
  world.observer<Transform>()
      .event(flecs::OnAdd)
      .event(flecs::OnSet)
      .each([](flecs::entity e, Transform &p) { e.add<TransformDirty>(); });
  // world.defer_end();
}

void TransformModule::RegisterDirtyTransformResolveSystem(flecs::world &world) {

  // clang-format off
  world.system<const Transform, WorldMatrix, const WorldMatrix *>("Dirtied Transform Updating")
  
  .term_at(2).parent().cascade().optional() // Get near parent's world matrix in a parent to child (bfs) order
  .kind(flecs::PreStore) // Comes after regular scene update which may have added dirtied tags
  .with<TransformDirty>().self().up() // with self or anscestor that has a TransformDirty tag
  // clang-format on 

  .each([](flecs::entity e, const Transform& local, WorldMatrix& wm, const WorldMatrix* parent_wm) {

          glm::mat4 local_mat =
              glm::translate(glm::mat4(1.0f), local.translation) *
              glm::mat4_cast(local.rotation) *
              glm::scale(glm::mat4(1.0f), local.scale);

              
          // Inherit parent world matrix
          if (parent_wm != nullptr) {
            wm.mat = parent_wm->mat * local_mat;
          } else {
            wm.mat = local_mat;
          }
          if (e.owns<TransformDirty>()) {
            e.remove<TransformDirty>();
          }
    });
}

TransformModule::TransformModule(flecs::world &world) {
  world.module<TransformModule>();

  world.component<glm::vec3>()
    .member<float>("x")
    .member<float>("y")
    .member<float>("z");

  world.component<glm::quat>()
    .member<float>("x")
    .member<float>("y")
    .member<float>("z")
    .member<float>("w");

  world.component<glm::mat4>();

  // Register components
  world.component<TransformDirty>();
  world.component<Transform>()
  .member<glm::vec3>("translation")
  .member<glm::quat>("rotation")
  .member<glm::vec3>("scale")
  .add(flecs::With, world.component<WorldMatrix>()); // Always have a WorldMatrix with Transform

  RegisterTransformDirtyObserver(world);
  // Register transform propogration system;
  RegisterDirtyTransformResolveSystem(world);
  // clang-format off
  // clang-format on 
}
} // namespace Expectre