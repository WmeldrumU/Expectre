#include "flecs/TransformModule.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Expectre {

TransformModule::TransformModule(flecs::world &world) {
  world.module<TransformModule>();
  // Register components
  world.component<Transform>();

  // Register transform propogration system;
  world.system<const Transform, WorldMatrix>("Transform Propogation")
      .term_at(0)
      .cascade()
      .detect_changes()
      .run([](flecs::iter &it) {
        // flecs tables group multiple entities together, becuase of this,
        // we have to check if the current entity has actually changed

        if (!it.changed()) {
          return;
        }

        const auto local_transforms = it.field<const Transform>(0);
        auto world_mats = it.field<WorldMatrix>(1);

        for (auto i : it) {
          flecs::entity ent = it.entity(i);
          const Transform &local = local_transforms[i];
          WorldMatrix& world_mat = world_mats[i];

          glm::mat4 local_mat = glm::translate(glm::mat4(1.0f), local.translation) *
        }
      });
}
} // namespace Expectre