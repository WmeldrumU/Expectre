
#include "scene/Scene.h"
#include "Mesh.h"
#include "scene/Component.h"
#include "scene/TransformComponent.h"
#include <stdexcept>

namespace Expectre {
Scene::Scene(std::string scene_name) {

  m_world.set<flecs::Rest>({});
  m_world.import<flecs::stats>();
  // REGISTER COMPONENTS HERE
  m_world.component<Transform>();
  m_world.component<PendingUpload>();
  m_world.component<MeshHandle>();
  // m_world.component(flecs::ChildOf);
  m_world.component<Material>();
  m_world.component<UsesMaterial>();

  m_world.component<UsesMesh>().add(flecs::Traversable).add(flecs::Exclusive);

  /*
  m_renderables = m_world.query_builder<Transform>()
                      .with<UsesMesh>(flecs::Wildcard)
                      .without<PendingUpload>()
                      .build();

  */
  /*
    m_pending_renderables = m_world.query_builder<Transform>()
                                .with<UsesMesh>(flecs::Wildcard)
                                .with<PendingUpload>()
                                .build();

    */

  m_renderables = m_world.query_builder<Transform, MeshHandle>()
                      .without<PendingUpload>()
                      .term_at(1)
                      .up<UsesMesh>()
                      .build();
  m_pending_renderables = m_world.query_builder<Transform, MeshHandle>()
                              .with<PendingUpload>()
                              .term_at(1)
                              .up<UsesMesh>()
                              .build();

  auto teapot_dir = WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj");
  auto bunny_dir = WORKSPACE_DIR + std::string("/assets/bunny.obj");
  auto lamp_dir =
      WORKSPACE_DIR + std::string("/assets/gltf/AnisotropyBarnLamp.glb");
  m_importer.import_model(teapot_dir, m_world);
  m_importer.import_model(bunny_dir, m_world);
  //  m_importer.import_model(lamp_dir, m_entities, m_pending_renderables);
}

void Scene::Update(uint64_t delta_time, const InputManager &input_manager) {
  m_world.progress();
  m_camera.update(delta_time, input_manager);
  // Update scene logic here, e.g., traverse entity list, update animations.
}

} // namespace Expectre