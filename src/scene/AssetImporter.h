#ifndef SCENE_ASSET_IMPORTER_H
#define SCENE_ASSET_IMPORTER_H

#include <assimp/Importer.hpp>
#include <assimp/defs.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <flecs.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <variant>

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
    // Create entity
    auto current_entity = world.entity(node->mName.C_Str());
    if (parent.is_valid()) {
      // set parent if parent is valid
      current_entity.child_of(parent);
    }

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
      auto *ai_mesh = scene->mMeshes[mesh_index];
      Mesh mesh{};
      mesh.name = ai_mesh->mName.C_Str();
      mesh.vertices.reserve(ai_mesh->mNumVertices);
      mesh.indices.reserve(ai_mesh->mNumFaces * 3);
      // Import mesh, to manager, create entity from the returned handle
      MeshHandle mesh_handle = MeshManager::Instance().import_mesh(mesh);
      flecs::entity mesh_ent =
          world.entity((std::string("MESH_") + mesh.name).c_str())
              .set<MeshHandle>({mesh_handle});

      // There are potentailly multiple meshes, import
      // each sub-mesh as a child entity
      std::string child_name = std::string("MESH_") + ai_mesh->mName.C_Str() +
                               "_" + std::to_string(i);
      flecs::entity child_entity =
          world.entity(child_name.c_str()).child_of(current_entity);

      // Add UsesMesh component to map child_entity -> mesh_entity
      child_entity.add<UsesMesh>(mesh_ent).add(flecs::Exclusive);
      // Add Transform component
      child_entity.set<Transform>({current_transform});
      child_entity.add<PendingUpload>();

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

        std::string mat_name =
            std::string("MAT_") + ai_material->GetName().C_Str();
        auto material_entity =
            world.entity(mat_name.c_str()).set<Material>({material});
        child_entity.add<UsesMaterial>(material_entity);

      } else {
        // Use default material if mesh doesn't have one
        Material default_material =
            TextureManager::Instance().get_default_material();
        auto material_entity =
            world.entity("MAT_default").set<Material>({default_material});
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
                       aiProcess_OptimizeGraph | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
      spdlog::error("ERROR::ASSIMP::{}", importer.GetErrorString());
      return;
    }

    const aiNode *ai_root = scene->mRootNode;

    import_model_helper(scene, ai_root, flecs::entity::null(), file_path,
                        world);
  }

  void process_gltf_node(const fastgltf::Asset &asset,
                         const fastgltf::Node &node,
                         const flecs::entity &parent, flecs::world &world) {

    // Create entity
    auto current_entity = world.entity(node.name.c_str());
    if (parent.is_valid()) {
      // set parent if parent is valid
      current_entity.child_of(parent);
    }
    const auto &gltf_trf = std::get<fastgltf::TRS>(node.transform);

    // Safe, direct memory loads for vectors
    auto glm_translation = glm::make_vec3(gltf_trf.translation.data());
    auto glm_scale = glm::make_vec3(gltf_trf.scale.data());

    // Manual assignment to accurately map glTF (X,Y,Z,W) array layout to GLM
    // (W,X,Y,Z)
    auto glm_rotation = glm::quat{gltf_trf.rotation[3], gltf_trf.rotation[0],
                                  gltf_trf.rotation[1], gltf_trf.rotation[2]};

    current_entity.set<Transform>(
        Transform(glm_translation, glm_rotation, glm_scale));

    if (auto idx = node.meshIndex; idx.has_value()) {
      current_entity.add<Mesh>();
      MeshHandle mesh_handle =
          MeshManager::Instance().import_mesh(asset.meshes[idx.value()]);
    }
  }

  void process_gltf_mesh(const fastgltf::Asset &asset,
                         const fastgltf::Mesh &mesh, flecs::world &world) {}

  void process_gltf_scene(const fastgltf::Asset &asset,
                          const fastgltf::Scene &scene, flecs::world &world) {
    for (auto node_idx : scene.nodeIndices) {

      process_gltf_node(asset, asset.nodes[node_idx], flecs::entity::null(),
                        world);
    }
  }

  void import_gltf_model(const std::string &file_path, flecs::world &world) {
    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
    if (data.error() != fastgltf::Error::None) {
      spdlog::error("import_gltf_model()0: {}",
                    fastgltf::getErrorMessage(data.error()));
      return;
    }

    std::filesystem::path _path(file_path);
    auto asset = m_parser.loadGltf(data.get(), _path.parent_path(),
                                   fastgltf::Options::DecomposeNodeMatrices |
                                       fastgltf::Options::GenerateMeshIndices);
    if (auto error = asset.error(); error != fastgltf::Error::None) {
      spdlog::error("import_gltf_model()1: {}",
                    fastgltf::getErrorMessage(data.error()));
      return;
    }

#if !defined(NDEBUG) || defined(_DEBUG)
    if (auto error = fastgltf::validate(asset.get());
        error != fastgltf::Error::None) {
      spdlog::error("import_gltf_model()2: {}",
                    fastgltf::getErrorMessage(data.error()));
      return;
    }
#endif

    for (const fastgltf::Mesh &gltf_mesh : asset->meshes) {
      auto mesh_handle =
          MeshManager::Instance().import_mesh(asset.get(), gltf_mesh);
    }

    for (const fastgltf::Material &gltf_mat : asset->materials) {
      auto mat_handle =
          TextureManager::Instance().import_material(asset.get(), gltf_mat);
    }

    for (const fastgltf::Scene scene : asset->scenes) {
      process_gltf_scene(asset.get(), scene, world);
    }
  }

private:
  fastgltf::Parser m_parser;
};

} // namespace Expectre

#endif // SCENE_ASSET_IMPORTER_H