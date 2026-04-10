#ifndef SCENE_ASSET_IMPORTER_H
#define SCENE_ASSET_IMPORTER_H

#include <assimp/Importer.hpp>
#include <assimp/defs.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <spdlog/spdlog.h>

#include "Entity.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "RenderableInfo.h"
#include "scene/MeshComponent.h"
#include "scene/TransformComponent.h"

namespace Expectre {
class AssetImporter {
public:
  // Import model at file path and append to entities and imported_renderables
  // Imported_renderables acts as a list draw calls for easy consumption when
  // rendering
  void import_model_helper(const aiScene *scene, const aiNode *node,
                           uint32_t parent, const std::string &file_path,
                           std::vector<Entity> &entities,
                           std::vector<RenderableInfo> &imported_renderables) {
    if (node == nullptr) {
      return;
    }

    uint32_t current_entity_idx = entities.size();
    entities.emplace_back(node->mName.C_Str(), parent);

    auto ai_trf = node->mTransformation;
    aiVector3D scale, translation;
    aiQuaternion rotation;
    ai_trf.Decompose(scale, rotation, translation);
    auto *trf_cpt =
        entities[current_entity_idx].add_component<TransformComponent>();
    trf_cpt->set_translation({translation.x, translation.y, translation.z});
    trf_cpt->set_rotation({rotation.w, rotation.x, rotation.y, rotation.z});
    trf_cpt->set_scale(glm::vec3{scale.x, scale.y, scale.z});

    // Create separate entities for each mesh.
    for (auto i = 0; i < node->mNumMeshes; i++) {
      auto mesh_index = node->mMeshes[i];
      auto ai_mesh = scene->mMeshes[mesh_index];

      // There are potentailly multiple meshes, import
      // each sub-mesh as a child entity
      uint32_t child_entity_idx = entities.size();

      entities.emplace_back(ai_mesh->mName.C_Str(), current_entity_idx);

      std::ignore =
          entities[child_entity_idx].add_component<TransformComponent>();
      auto *mesh_cpt =
          entities[child_entity_idx].add_component<MeshComponent>();

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
      imported_renderables.push_back(info);
    }

    // Recurse for child nodes
    for (auto i = 0; i < node->mNumChildren; i++) {
      auto ai_child_node = node->mChildren[i];
      import_model_helper(scene, ai_child_node, current_entity_idx, file_path,
                          entities, imported_renderables);
    }
  }

  void
  import_model(const std::string &file_path,
                         std::vector<Entity> &entities,
                         std::vector<RenderableInfo> &imported_renderables) {
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

    import_model_helper(scene, ai_root, /*parent*/ kInvalidEntity, file_path,
                        entities, imported_renderables);
  }

private:
};

} // namespace Expectre

#endif // SCENE_ASSET_IMPORTER_H