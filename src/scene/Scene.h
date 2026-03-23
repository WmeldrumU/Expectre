#ifndef SCENE
#define SCENE

#include <filesystem>
#include <spdlog/spdlog.h>

#include "MaterialManager.h"
#include "MeshManager.h"
#include "RenderableInfo.h"
#include "ResourceManager.h"
#include "input/InputManager.h"
#include "scene/Camera.h"
#include "scene/Entity.h"
#include "scene/MeshComponent.h"
#include "scene/SceneObject.h"
#include "scene/SceneRoot.h"
#include "scene/TransformComponent.h"

#include <assimp/Importer.hpp>
#include <assimp/defs.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

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
  void create_root();

  EntityId create_and_register_entity(std::string name,
                                      EntityId parent = kInvalidEntity) {
    EntityId id = static_cast<EntityId>(m_entities.size());
    m_entities.emplace_back(std::move(name), parent);
    return id;
  }

  void import_as_entity(const aiScene *scene, const aiNode *node,
                        EntityId parent, const std::string &file_path = "") {
    if (node == nullptr) {
      return;
    }
    // convert ai node to entity, separating out the meshes to their own
    // entities Create entity object for this node
    EntityId current_entity_id =
        create_and_register_entity(node->mName.C_Str(), parent);

    auto ai_trf = node->mTransformation;

    aiVector3D scale, translation;
    aiQuaternion rotation;
    ai_trf.Decompose(scale, rotation, translation);
    auto *trf_cpt =
        m_entities[current_entity_id].add_component<TransformComponent>();
    trf_cpt->set_translation({translation.x, translation.y, translation.z});
    trf_cpt->set_rotation({rotation.w, rotation.x, rotation.y, rotation.z});
    trf_cpt->set_scale(glm::vec3{scale.x, scale.y, scale.z});

    // Create separate entities for each mesh.
    for (auto i = 0; i < node->mNumMeshes; i++) {
      auto mesh_index = node->mMeshes[i];
      auto ai_mesh = scene->mMeshes[mesh_index];

      // There are potentailly multiple meshes, import
      // each sub-mesh as a child entity
      EntityId child_ent_id =
          create_and_register_entity(ai_mesh->mName.C_Str(), current_entity_id);
      std::ignore =
          m_entities[child_ent_id].add_component<TransformComponent>();
      auto *mesh_cpt = m_entities[child_ent_id].add_component<MeshComponent>();

      // // Import mesh
      MeshHandle mesh_handle = MeshManager::Instance().import_mesh(ai_mesh);

      MaterialHandle material_handle;
      // Import material if mesh has one, otherwise use default
      if (ai_mesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial *ai_material =
            scene->mMaterials[ai_mesh->mMaterialIndex];

        // Extract model directory for texture loading
        std::string model_directory =
            file_path.empty()
                ? ""
                : std::filesystem::path(file_path).parent_path().string();
        material_handle = MaterialManager::Instance().import_material(
            scene, ai_material, model_directory);

      } else {
        // Use default material if mesh doesn't have one
        material_handle = MaterialManager::Instance().get_default_material();
      }

      // Set the mesh and material
      mesh_cpt->set_material(material_handle);
      mesh_cpt->set_mesh(mesh_handle);
      RenderableInfo info;
      info.mesh = mesh_handle;
      info.material = material_handle;
      m_pending_renderables.push_back(info);
    }

    // Recurse for child nodes
    for (auto i = 0; i < node->mNumChildren; i++) {
      auto ai_child_node = node->mChildren[i];
      import_as_entity(scene, ai_child_node, current_entity_id, file_path);
    }
  }

  void import_model_as_entity(const std::string &file_path) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(
        file_path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                       aiProcess_CalcTangentSpace |
                       aiProcess_ImproveCacheLocality);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
      spdlog::error("ERROR::ASSIMP::{}", importer.GetErrorString());
      return;
    }

    const aiNode *ai_root = scene->mRootNode;

    import_as_entity(scene, ai_root, /*parent*/ kInvalidEntity, file_path);
  }

  SceneRoot m_root;
  Camera m_camera;

  // ECS
  std::vector<Entity> m_entities;

  // Entites whose GPU resources haven't been uploaded yet.
  // Each entity is added once in import_as_entity, consumed once by renderer
  std::vector<RenderableInfo> m_pending_renderables;
};
} // namespace Expectre
#endif // SCENE