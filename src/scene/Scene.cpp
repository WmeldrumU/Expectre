
#include "scene/Scene.h"
#include <stdexcept>

namespace Expectre {
Scene::Scene(std::string scene_name) {

  auto teapot_dir = WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj");
  auto bunny_dir = WORKSPACE_DIR + std::string("/assets/bunny.obj");
  auto lamp_dir =
      WORKSPACE_DIR + std::string("/assets/gltf/AnisotropyBarnLamp.glb");
  m_importer.import_model(teapot_dir, m_entities, m_pending_renderables);
  m_importer.import_model(bunny_dir, m_entities, m_pending_renderables);
  // m_importer.import_model(lamp_dir, m_entities, m_pending_renderables);
}

std::vector<RenderableInfo> Scene::gather_renderables() const {
  std::vector<RenderableInfo> result;
  result.reserve(m_entities.size());

  for (const auto &ent : m_entities) {
    const auto *mesh_cpt = ent.get_component<MeshComponent>();
    if (!mesh_cpt) {
      continue;
    }

    const auto *trf_cpt = ent.get_component<TransformComponent>();

    RenderableInfo info;
    info.mesh = mesh_cpt->get_mesh();
    info.material = mesh_cpt->get_material();
    info.transform =
        trf_cpt ? trf_cpt->get_transform_matrix() : glm::mat4(1.0f);
    result.push_back(info);
  }
  return result;
}

void Scene::Update(uint64_t delta_time, const InputManager &input_manager) {

  m_camera.update(delta_time, input_manager);
  // Update scene logic here, e.g., traverse entity list, update animations.
}

} // namespace Expectre