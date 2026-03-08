
#include "scene/Scene.h"
#include "scene/SceneRoot.h"

#include <stdexcept>

namespace Expectre {
Scene::Scene(std::string scene_name)
    : m_root{}, m_camera{&m_root, scene_name + "Camera"} {

  auto teapot_dir = WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj");
  auto bunny_dir = WORKSPACE_DIR + std::string("/assets/bunny.obj");
  Model::import_model_as_scene_object(teapot_dir, m_root);
  Model::import_model_as_scene_object(bunny_dir, m_root);
}

void Scene::Update(uint64_t delta_time, const InputManager &input_manager) {

  m_camera.update(delta_time, input_manager);
  // Update scene logic here, e.g., traverse scene graph, update animations,
  // etc. This is a placeholder implementation.
  (void)delta_time;    // suppress unused parameter warning
  (void)input_manager; // suppress unused parameter warning
}

} // namespace Expectre