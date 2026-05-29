
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
  // m_world.component(flecs::ChildOf);
  m_world.component<Material>();
  m_world.component<UsesMaterial>().add(flecs::Traversable);

  m_world.component<MeshHandle>();
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

  m_renderables = m_world.query_builder<Transform, MeshHandle, Material>()
                      .without<PendingUpload>()
                      .term_at(1)
                      .up<UsesMesh>()
                      .term_at(2)
                      .up<UsesMaterial>()
                      .build();
  m_pending_renderables =
      m_world.query_builder<Transform, MeshHandle, Material>()
          .with<PendingUpload>()
          .term_at(1)
          .up<UsesMesh>()
          .term_at(2)
          .up<UsesMaterial>()
          .build();

  auto teapot_dir = WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj");
  auto bunny_dir = WORKSPACE_DIR + std::string("/assets/bunny.obj");
  auto lamp_dir =
      WORKSPACE_DIR + std::string("/assets/gltf/AnisotropyBarnLamp.glb");
  // m_importer.import_model(teapot_dir, m_world);
  // m_importer.import_model(bunny_dir, m_world);
  // m_importer.import_gltf_model(usd_file_dir, m_world);
}

void Scene::Update(uint64_t delta_time, const InputManager &input_manager) {
  m_world.progress();
  m_camera.update(delta_time, input_manager);
  // Update scene logic here, e.g., traverse entity list, update animations.
}

} // namespace Expectre