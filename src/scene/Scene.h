#ifndef SCENE
#define SCENE

#include "AssetImporter.h"
#include "MeshManager.h"
#include "RenderableInfo.h"
#include "TextureManager.h"
#include "input/InputManager.h"
#include "scene/Camera.h"
#include "scene/Entity.h"
#include "scene/MeshComponent.h"
#include "scene/TransformComponent.h"
#include <flecs.h>
#include <spdlog/spdlog.h>
#include <vector>

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
  const Camera &get_camera() { return m_camera; }

  std::vector<RenderableInfo> consume_pending_renderables() {
    std::vector<RenderableInfo> pending;
    pending.reserve(m_pending_renderables.count());

    // We must defer the operations done on enitities because of the
    // call to remove(). Removing mid-iteration would move the entitiy to a
    // different table. This is the flecs equivalent of iterating and altering at
    // the same time

    m_world.defer_begin();

    m_pending_renderables.each([&](flecs::entity e, Transform trf,
                                   MeshHandle mh) {
      pending.emplace_back(mh,
                           TextureManager::Instance().get_default_material(),
                           trf.get_transform_matrix());

      e.remove<PendingUpload>();
    });

    m_world.defer_end();

    return pending;
  }

  std::vector<RenderableInfo> gather_renderables() {
    std::vector<RenderableInfo> pending;

    m_renderables.each([&](Transform trf, MeshHandle mh) {
      pending.emplace_back(mh,
                           TextureManager::Instance().get_default_material(),
                           trf.get_transform_matrix());
    });
    return pending;
  }

private:
  Camera m_camera;
  AssetImporter m_importer;
  // ECS
  // ROOT IS STORED AS THE FIRST ELEMENT
  // std::vector<Entity> m_entities;
  flecs::world m_world;
  flecs::query<Transform, MeshHandle> m_renderables;
  flecs::query<Transform, MeshHandle> m_pending_renderables;

  void foo() {}
};
} // namespace Expectre
#endif // SCENE