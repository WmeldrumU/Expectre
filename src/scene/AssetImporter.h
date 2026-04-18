#ifndef SCENE_ASSET_IMPORTER_H
#define SCENE_ASSET_IMPORTER_H

#include <assimp/Importer.hpp>
#include <assimp/defs.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <flecs.h>
#include <spdlog/spdlog.h>

#include "Component.h"
#include "Entity.h"
#include "MeshManager.h"
#include "RenderableInfo.h"
#include "TextureManager.h"
#include "scene/MeshComponent.h"
#include "scene/TransformComponent.h"

namespace Expectre {
class AssetImporter {
public:
  // Import model at file path and append to entities and imported_renderables
  // Imported_renderables acts as a list draw calls for easy consumption when
  // rendering
  void import_model_helper(const aiScene *scene, const aiNode *node,
                           const flecs::entity &parent,
                           const std::string &file_path, flecs::world &world) {
    if (node == nullptr) {
      return;
    }

    // Create entity and set parent
    auto current_entity = world.entity(node->mName.C_Str()).child_of(parent);

    // Get transform from assimp matrix
    auto ai_trf = node->mTransformation;
    aiVector3D scale, translation;
    aiQuaternion rotation;
    ai_trf.Decompose(scale, rotation, translation);

    // Convert assimp transform to glm
    auto glm_translation =
        glm::vec3{translation.x, translation.y, translation.z};
    auto glm_rotation =
        glm::quat{rotation.w, rotation.x, rotation.y, rotation.z};
    auto glm_scale = glm::vec3{scale.x, scale.y, scale.z};

    const auto current_transform =
        Transform{glm_translation, glm_rotation, glm_scale};
    // Add this transform as a component to current entity
    current_entity.set<Transform>(current_transform);

    // Create separate entities for each mesh.
    for (auto i = 0; i < node->mNumMeshes; i++) {
      auto mesh_index = node->mMeshes[i];
      auto ai_mesh = scene->mMeshes[mesh_index];

      // Import mesh, to manager, create entity from the returned handle
      MeshHandle mesh_handle = MeshManager::Instance().import_mesh(ai_mesh);
      flecs::entity mesh_ent = world.entity().set<MeshHandle>({mesh_handle});

      // There are potentailly multiple meshes, import
      // each sub-mesh as a child entity
      std::string child_name =
          std::string(node->mName.C_Str()) + "_childmesh_" + std::to_string(i);
      flecs::entity child_entity =
          world.entity(child_name.c_str()).child_of(current_entity);

      // Add UsesMesh component to map child_entity -> mesh_entity
      child_entity.add<UsesMesh>(mesh_ent).add(flecs::Exclusive);
      // Add Transform component
      child_entity.set<Transform>({current_transform});

      // Import material if mesh has one, otherwise use default
      if (ai_mesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial *ai_material =
            scene->mMaterials[ai_mesh->mMaterialIndex];

        // Extract model directory for texture loading
        std::string model_directory =
            file_path.empty()
                ? ""
                : std::filesystem::path(file_path).parent_path().string();
        Material material = TextureManager::Instance().import_material(
            scene, ai_material, model_directory);

        auto material_entity = world.entity().set<Material>({material});
        child_entity.add<UsesMaterial>(material_entity);

      } else {
        // Use default material if mesh doesn't have one
        TextureHandle default_tex =
            TextureManager::Instance().get_default_texture();
        Material material = {"_DEFAULT_MATERIAL_", default_tex, default_tex,
                             default_tex,          default_tex, default_tex};
        auto material_entity = world.entity().set<Material>({material});
        child_entity.add<UsesMaterial>(material_entity);
      }
    }

    // Recurse for child nodes
    for (auto i = 0; i < node->mNumChildren; i++) {
      auto ai_child_node = node->mChildren[i];
      import_model_helper(scene, ai_child_node, current_entity, file_path,
                          world);
    }
  }

  void import_model(const std::string &file_path, flecs::world &world) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(
        file_path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                       aiProcess_CalcTangentSpace |
                       aiProcess_ImproveCacheLocality |
                       aiProcess_OptimizeGraph);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
      spdlog::error("ERROR::ASSIMP::{}", importer.GetErrorString());
      return;
    }

    const aiNode *ai_root = scene->mRootNode;
    auto root_entity = world.entity(ai_root->mName.C_Str());

    for (unsigned i = 0; i < ai_root->mNumChildren; i++) {
      import_model_helper(scene, ai_root->mChildren[i], root_entity, file_path,
                          world);
    }
  }

private:
};

} // namespace Expectre

#endif // SCENE_ASSET_IMPORTER_H