#ifndef SCENE
#define SCENE

#include <spdlog/spdlog.h>
#include "MaterialManager.h"
#include "MeshManager.h"
#include "TextureManager.h"

#include "AssetImporter.h"
#include "RenderableInfo.h"
#include "input/InputManager.h"
#include "scene/Camera.h"
#include "scene/Entity.h"
#include "scene/MeshComponent.h"

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

  const std::vector<Entity> &get_entities() { return m_entities; }
  std::vector<RenderableInfo> consume_pending_renderables() {
    std::vector<RenderableInfo> out;
    out.swap(m_pending_renderables);
    return out;
  }

  std::vector<RenderableInfo> gather_renderables() const;

  const Entity &get_entity(const EntityId &id) { return m_entities[id]; }

private:
  EntityId create_and_register_entity(std::string name,
                                      EntityId parent = kInvalidEntity) {
    EntityId id = static_cast<EntityId>(m_entities.size());
    m_entities.emplace_back(std::move(name), parent);
    return id;
  }

  Camera m_camera;
  AssetImporter m_importer;
  // ECS
  // ROOT IS STORED AS THE FIRST ELEMENT
  std::vector<Entity> m_entities;

  // Entities whose GPU resources haven't been uploaded yet.
  // Each entity is added once in AssetImporter::import_model, consumed once by the renderer
  std::vector<RenderableInfo> m_pending_renderables;
};
} // namespace Expectre
#endif // SCENE