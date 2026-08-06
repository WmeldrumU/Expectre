#include "flecs/CameraModule.h"
#include "flecs/TransformModule.h"
#include "input/InputManager.h"

namespace Expectre {

CameraModule::CameraModule(flecs::world &world) {
  world.module<CameraModule>();

  // Register components
  world.component<Camera>()
      .add(flecs::Singleton)
      .add(flecs::With, world.component<Transform>("with-trf"));
  world.set<Camera>({});

  // world.entity("DebugCameraInstance").add<Camera>();

  register_camera_system(world);
  // clang-format off
  // clang-format on 
}

void CameraModule::register_camera_system(flecs::world& world) {

  const InputManager& input_manager = world.get<InputManager>();

  // clang-format off
  // clang-format on

  world.system<const Camera, Transform>("Camera Input Handling System")
      .kind(flecs::OnUpdate)
      .each([](flecs::entity e, const Camera &cam, Transform &trf) {
        const InputManager &input_manager = e.world().get<InputManager>();
        const auto dt = e.world().delta_time();

        // multiply our rotation quat by the standard forward vector
        glm::vec3 forward = trf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(forward, world_up));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        // Calculate movement direction based on camera's local axes
        glm::vec3 movement_vector(0.0f);

        if (input_manager.is_scancode_down(SDL_SCANCODE_W))
          movement_vector += forward; // Move forward
        if (input_manager.is_scancode_down(SDL_SCANCODE_S))
          movement_vector -= forward; // Move backward
        if (input_manager.is_scancode_down(SDL_SCANCODE_A))
          movement_vector -= right; // Move left
        if (input_manager.is_scancode_down(SDL_SCANCODE_D))
          movement_vector += right; // Move right
        if (input_manager.is_scancode_down(SDL_SCANCODE_Q))
          movement_vector -= up;
        if (input_manager.is_scancode_down(SDL_SCANCODE_E))
          movement_vector += up;

        // Normalize direction to prevent faster diagonal movement
        if (glm::length(movement_vector) > 0.0f)
          movement_vector = glm::normalize(movement_vector);

        // Update position with delta time for frame-rate independent movement
        trf.translation +=
            movement_vector * cam.speed * static_cast<float>(dt) / 1000.0f;
      });
}
} // namespace Expectre