
#include "scene/Scene.h"
#include "Mesh.h"
#include "scene/Component.h"
#include "scene/TransformComponent.h"
#include <stdexcept>

namespace Expectre {
Scene::Scene(std::string scene_name) {
  m_world.set<flecs::Rest>({});

  m_renderables = m_world.query_builder<Transform, MeshHandle>()
                      .without<PendingUpload>()
                      .term_at(1)
                      .src()
                      .second<UsesMesh>()
                      .build();

  m_pending_renderables = m_world.query_builder<Transform, MeshHandle>()
                              .with<PendingUpload>()
                              .term_at(1)
                              .src()
                              .second<UsesMesh>()
                              .build();

  auto teapot_dir = WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj");
  auto bunny_dir = WORKSPACE_DIR + std::string("/assets/bunny.obj");
  auto lamp_dir =
      WORKSPACE_DIR + std::string("/assets/gltf/AnisotropyBarnLamp.glb");
  m_importer.import_model(teapot_dir, m_world);

  // Create a second teapot instance sharing the same mesh/material data
  // m_importer.import_model(bunny_dir, m_entities, m_pending_renderables);
  //  m_importer.import_model(lamp_dir, m_entities, m_pending_renderables);
}

void Scene::Update(uint64_t delta_time, const InputManager &input_manager) {

  m_camera.update(delta_time, input_manager);
  // Update scene logic here, e.g., traverse entity list, update animations.
}

} // namespace Expectre