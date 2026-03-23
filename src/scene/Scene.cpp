
#include "scene/Scene.h"
#include "scene/CameraComponent.h"
#include "scene/SceneRoot.h"

#include <stdexcept>

namespace Expectre {
Scene::Scene(std::string scene_name)
    : m_root{}, m_camera{&m_root, scene_name + "Camera"} {

  auto teapot_dir = WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj");
  auto bunny_dir = WORKSPACE_DIR + std::string("/assets/bunny.obj");
  import_model_as_entity(teapot_dir);
  import_model_as_entity(bunny_dir);

  // m_camera.add_component<CameraComponent>();
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
        trf_cpt ? trf_cpt->get_matrix() : glm::mat4(1.0f);
    result.push_back(info);
  }
  return result;
}

void Scene::Update(uint64_t delta_time, const InputManager &input_manager) {

  m_camera.update(delta_time, input_manager);
  // auto *camera_cpt = m_camera.get_component<CameraComponent>();
  // camera_cpt->update(delta_time, input_manager);
  // Update scene logic here, e.g., traverse scene graph, update animations,
  // etc. This is a placeholder implementation.
}

} // namespace Expectre