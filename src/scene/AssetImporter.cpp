
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
#define STB_IMAGE_IMPLEMENTATION // includes stb function bodies
#include "Image.h"
#include "Material.h"
#include "Mesh.h"
#include "RenderableInfo.h"
#include "scene/AssetImporter.h"
#include "scene/TransformComponent.h"

namespace Expectre {

// void AssetImporter::import_model_helper(const aiScene *scene,
//                                         const aiNode *node,
//                                         const flecs::entity &parent,
//                                         const std::string &file_path,
//                                         flecs::world &world) {
//   if (node == nullptr) {
//     return;
//   }
//   // Create entity
//   auto current_entity = world.entity(node->mName.C_Str());
//   if (parent.is_valid()) {
//     // set parent if parent is valid
//     current_entity.child_of(parent);
//   }

//   // Get transform from assimp matrix
//   auto ai_trf = node->mTransformation;
//   aiVector3D scale, translation;
//   aiQuaternion rotation;
//   ai_trf.Decompose(scale, rotation, translation);

//   // Convert assimp transform to glm
//   auto glm_translation = glm::vec3{translation.x, translation.y,
//   translation.z}; auto glm_rotation = glm::quat{rotation.w, rotation.x,
//   rotation.y, rotation.z}; auto glm_scale = glm::vec3{scale.x, scale.y,
//   scale.z};

//   const auto current_transform =
//       Transform{glm_translation, glm_rotation, glm_scale};
//   // Add this transform as a component to current entity
//   current_entity.set<Transform>(current_transform);

//   // Create separate entities for each mesh.
//   for (auto i = 0; i < node->mNumMeshes; i++) {
//     auto mesh_index = node->mMeshes[i];
//     auto *ai_mesh = scene->mMeshes[mesh_index];
//     Mesh mesh{};
//     mesh.name = ai_mesh->mName.C_Str();
//     mesh.vertices.reserve(ai_mesh->mNumVertices);
//     mesh.indices.reserve(ai_mesh->mNumFaces * 3);
//     // Import mesh, to manager, create entity from the returned handle
//     MeshHandle mesh_handle = MeshManager::Instance().import_mesh(mesh);
//     flecs::entity mesh_ent =
//         world.entity((std::string("MESH_") + mesh.name).c_str())
//             .set<MeshHandle>({mesh_handle});

//     // There are potentailly multiple meshes, import
//     // each sub-mesh as a child entity
//     std::string child_name =
//         std::string("MESH_") + ai_mesh->mName.C_Str() + "_" +
//         std::to_string(i);
//     flecs::entity child_entity =
//         world.entity(child_name.c_str()).child_of(current_entity);

//     // Add UsesMesh component to map child_entity -> mesh_entity
//     child_entity.add<UsesMesh>(mesh_ent).add(flecs::Exclusive);
//     // Add Transform component
//     child_entity.set<Transform>({current_transform});
//     child_entity.add<PendingUpload>();

//     // Import material if mesh has one, otherwise use default
//     if (ai_mesh->mMaterialIndex < scene->mNumMaterials) {
//       const aiMaterial *ai_material =
//           scene->mMaterials[ai_mesh->mMaterialIndex];

//       // Extract model directory for texture loading
//       std::string model_directory =
//           file_path.empty()
//               ? ""
//               : std::filesystem::path(file_path).parent_path().string();
//       Material material = TextureManager::Instance().import_material(
//           scene, ai_material, model_directory);

//       std::string mat_name =
//           std::string("MAT_") + ai_material->GetName().C_Str();
//       auto material_entity =
//           world.entity(mat_name.c_str()).set<Material>({material});
//       child_entity.add<UsesMaterial>(material_entity);

//     } else {
//       // Use default material if mesh doesn't have one
//       Material default_material =
//           TextureManager::Instance().get_default_material();
//       auto material_entity =
//           world.entity("MAT_default").set<Material>({default_material});
//       child_entity.add<UsesMaterial>(material_entity);
//     }
//   }

//   // Recurse for child nodes
//   for (auto i = 0; i < node->mNumChildren; i++) {
//     auto ai_child_node = node->mChildren[i];
//     import_model_helper(scene, ai_child_node, current_entity, file_path,
//     world);
//   }
// }

void AssetImporter::process_gltf_node(const fastgltf::Asset &asset,
                                      flecs::entity file_entity,
                                      const size_t &node_idx,
                                      const flecs::entity &parent,
                                      flecs::world &world) {

  const GltfFile &gltf_file = file_entity.get<GltfFile>();

  const fastgltf::Node &node = asset.nodes[node_idx];

  // Create entity
  std::string safe_name = sanitize_name(node.name.c_str(), node_idx);
  auto current_node =
      world.entity(safe_name.c_str()).child_of(parent).add<Node>();

  const auto &gltf_trf = std::get<fastgltf::TRS>(node.transform);

  // Safe, direct memory loads for vectors
  auto glm_translation = glm::make_vec3(gltf_trf.translation.data());
  auto glm_scale = glm::make_vec3(gltf_trf.scale.data());

  // Manual assignment to accurately map glTF (X,Y,Z,W) array layout to GLM
  // (W,X,Y,Z)
  auto glm_rotation = glm::quat{gltf_trf.rotation[3], gltf_trf.rotation[0],
                                gltf_trf.rotation[1], gltf_trf.rotation[2]};

  current_node.set<Transform>(
      Transform(glm_translation, glm_rotation, glm_scale));

  if (auto mesh_idx = node.meshIndex; mesh_idx.has_value()) {
    flecs::entity mesh_ent = gltf_file.meshes[mesh_idx.value()];
    current_node.add<UsesMesh>(mesh_ent);
  }

  // Recurse down to children
  for (auto child_idx : node.children) {
    process_gltf_node(asset, file_entity, child_idx, current_node, world);
  }
}

std::string AssetImporter::sanitize_name(const std::string &name,
                                         size_t index) {
  if (name.empty()) {
    return "id_" + std::to_string(index);
  }
  std::string out_name;
  for (char c : name) {
    if (c == ' ' || c == ':' || c == '.' || c == '/') {
      out_name += c;
    }
  }
  return out_name;
}

void AssetImporter::import_gltf_meshes(const fastgltf::Asset &asset,
                                       flecs::entity &file_entity,
                                       flecs::world &world) {

  GltfFile &gltf_file = file_entity.get_mut<GltfFile>();
  gltf_file.meshes.resize(asset.meshes.size());
  for (size_t i = 0; i < asset.meshes.size(); i++) {

    const fastgltf::Mesh &gltf_mesh = asset.meshes[i];
    std::string mesh_name = "mesh_" + sanitize_name(gltf_mesh.name.c_str(), i);

    flecs::entity mesh_ent = world.entity(mesh_name.c_str())
                                 .child_of(file_entity)
                                 .set<Mesh>({mesh_name});

    for (size_t j = 0; j < gltf_mesh.primitives.size(); j++) {
      const fastgltf::Primitive &gltf_prim = gltf_mesh.primitives[j];
      if (gltf_prim.type != fastgltf::PrimitiveType::Triangles) {
        continue;
      }
      Primitive prim;
      PendingPrimitiveUpload pending_prim_upload;

      // Link material entity if present
      if (gltf_prim.materialIndex.has_value()) {
        prim.material = gltf_file.materials[gltf_prim.materialIndex.value()];
      }

      if (gltf_prim.indicesAccessor.has_value()) {
        // Read indices
        const auto &index_accessor =
            asset.accessors[gltf_prim.indicesAccessor.value()];

        pending_prim_upload.indices.resize(index_accessor.count);
        fastgltf::iterateAccessorWithIndex<uint32_t>(
            asset, index_accessor, [&](uint32_t raw_index, size_t out_idx) {
              pending_prim_upload.indices[out_idx] = raw_index;
            });
      }

      // Position
      const auto *pos_attr = gltf_prim.findAttribute("POSITION");
      if (pos_attr != gltf_prim.attributes.end()) {
        const fastgltf::Accessor &positions_accessor =
            asset.accessors[pos_attr->accessorIndex];
        pending_prim_upload.vertices.resize(positions_accessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, positions_accessor,
            [&](glm::vec3 pos, size_t idx) {
              pending_prim_upload.vertices[idx].pos = pos;
            }

        );
      }

      // Normals
      const auto *norm_attr = gltf_prim.findAttribute("NORMAL");
      if (norm_attr != gltf_prim.attributes.end()) {
        const auto &norm_accessor = asset.accessors[norm_attr->accessorIndex];
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, norm_accessor,
            [&](glm::vec3 normal, size_t idx) {
              pending_prim_upload.vertices[idx].normal = normal;
            }

        );
      } else {
        compute_vertex_normals(pending_prim_upload);
      }

      // UV Coords
      const auto *uv_attr = gltf_prim.findAttribute("TEXCOORD_0");
      if (uv_attr != gltf_prim.attributes.end()) {
        const auto &uv_accessor = asset.accessors[uv_attr->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset, uv_accessor, [&](glm::vec2 uv, size_t idx) {
              pending_prim_upload.vertices[idx].tex_coord = uv;
            });
      }

      // Vertex color
      const auto *vert_color_attr = gltf_prim.findAttribute("COLOR_0");
      if (vert_color_attr != gltf_prim.attributes.end()) {
        const auto &vert_color_accessor =
            asset.accessors[vert_color_attr->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, vert_color_accessor, [&](glm::vec3 color, size_t idx) {
              pending_prim_upload.vertices[idx].color = color;
            });
      }

      // Create primitive child entity
      std::string prim_name = mesh_name + "_prim_" + std::to_string(j);

      world.entity(prim_name.c_str())
          .child_of(mesh_ent)
          .set<Primitive>(std::move(prim))
          .set<PendingPrimitiveUpload>(std::move(pending_prim_upload));
    }

    gltf_file.meshes[i] = mesh_ent;
  }
  // Signal flecs that we finished modifing a component
  file_entity.modified<GltfFile>();
}

void AssetImporter::import_gltf_materials(const fastgltf::Asset &asset,
                                          flecs::entity &file_entity,
                                          flecs::world &world) {
  GltfFile &gltf_file = file_entity.get_mut<GltfFile>();
  gltf_file.materials.resize(asset.materials.size());

  for (size_t i = 0; i < asset.materials.size(); i++) {

    const fastgltf::Material &gltf_material = asset.materials[i];
    std::string mat_name =
        "mat_" + sanitize_name(gltf_material.name.c_str(), i);

    Material material;
    material.name = mat_name;

    // get scalar factors
    material.albedo_factor =
        glm::make_vec4(gltf_material.pbrData.baseColorFactor.data());
    material.metallic_factor = gltf_material.pbrData.metallicFactor;
    material.roughness_factor = gltf_material.pbrData.roughnessFactor;
    material.emissive_factor =
        glm::make_vec3(gltf_material.emissiveFactor.data());
    material.emissive_strength = gltf_material.emissiveStrength;

    auto get_img_idx = [&](const auto &tex_info) -> size_t {
      auto tex_idx = tex_info.textureIndex;
      return asset.textures[tex_idx].imageIndex.value_or(0);
    };
    // get textures
    // albedo
    if (gltf_material.pbrData.baseColorTexture.has_value()) {
      const auto idx =
          get_img_idx(gltf_material.pbrData.baseColorTexture.value());

      material.albedo = gltf_file.images[idx];
    }

    // metallic roughness
    if (gltf_material.pbrData.metallicRoughnessTexture.has_value()) {
      const auto idx =
          get_img_idx(gltf_material.pbrData.metallicRoughnessTexture.value());
      material.metallic_roughness = gltf_file.images[idx];
    }
    // normal
    if (gltf_material.normalTexture.has_value()) {
      const auto idx = get_img_idx(gltf_material.normalTexture.value());
      material.normal = gltf_file.images[idx];
    }

    // occlusion
    if (gltf_material.occlusionTexture.has_value()) {
      const auto idx = get_img_idx(gltf_material.occlusionTexture.value());
      material.occlusion = gltf_file.images[idx];
    }
    // emissive
    if (gltf_material.emissiveTexture.has_value()) {
      const auto idx = get_img_idx(gltf_material.emissiveTexture.value());
      material.emissive = gltf_file.images[idx];
    }

    world.entity(mat_name.c_str())
        .child_of(file_entity)
        .set<Material>(std::move(material));
  }
  // Signal flecs that we finished modifing a component
  file_entity.modified<GltfFile>();
}

void AssetImporter::import_gltf_images(
    const fastgltf::Asset &asset, flecs::entity &file_entity,
    flecs::world &world, const std::filesystem::path &canonical_path) {

  GltfFile &gltf_file = file_entity.get_mut<GltfFile>();
  gltf_file.images.resize(asset.materials.size());
  for (size_t i = 0; i < asset.images.size(); i++) {

    const auto &image = asset.images[i];

    std::string img_name = "img_" + sanitize_name(image.name.c_str(), i);
    Image result_image;

    // Extract raw data
    std::visit(
        fastgltf::visitor{
            [&](const std::monostate &) {
              spdlog::error("glTF image {} has no data source", img_name);
            },

            [&](const fastgltf::sources::URI &source) {
              if (!source.uri.isLocalPath()) {
                spdlog::error("glTF image {} uses non-local URI", img_name);
                return;
              }

              if (source.fileByteOffset != 0) {
                spdlog::error(
                    "glTF image {} has fileByteOffset != 0, unsupported",
                    img_name);
                return;
              }

              // if canonical path is something like
              // "/assets/models/character.gltf",
              // source.uri.path() is something  like "textures/image.png"
              // making full_path "/assets/model/textures/image.png"
              const auto full_path = std::filesystem::canonical(
                  canonical_path.parent_path() / source.uri.path());

              result_image = import_from_file(full_path);
            },

            [&](const fastgltf::sources::Array &source) {
              result_image = import_from_memory(
                  img_name,
                  reinterpret_cast<const uint8_t *>(source.bytes.data()),
                  source.bytes.size());
            },

            [&](const fastgltf::sources::BufferView &source) {
              const auto &buffer_view =
                  asset.bufferViews[source.bufferViewIndex];
              const auto &buffer = asset.buffers[buffer_view.bufferIndex];

              std::visit(
                  fastgltf::visitor{
                      [&](const fastgltf::sources::Array &buffer_source) {
                        const uint8_t *start =
                            reinterpret_cast<const uint8_t *>(
                                buffer_source.bytes.data() +
                                buffer_view.byteOffset);

                        result_image = import_from_memory(
                            img_name, start, buffer_view.byteLength);
                      },

                      [&](const auto &) {
                        spdlog::error(
                            "Unsupported buffer source for glTF image {}",
                            img_name);
                      }},
                  buffer.data);
            },

            [&](const auto &) {
              spdlog::error("Unsupported source for glTF image {}", img_name);
            }},
        image.data);

    auto image_entity = world.entity(img_name.c_str()).child_of(file_entity);
    gltf_file.images[i] = image_entity;

    if (result_image.data != nullptr) {
      result_image.name = img_name;
      image_entity.add<Image>(std::move(result_image));
    }
  }

  // Signal flecs that we finished modifing a component
  file_entity.modified<GltfFile>();
}

// todo: process all scenes and add scene to entity naming
void AssetImporter::process_gltf_scenes(const fastgltf::Asset &asset,
                                        flecs::entity &file_entity,
                                        flecs::world &world) {
  size_t active_scene_idx = asset.defaultScene.value_or(0);

  if (active_scene_idx < asset.scenes.size()) {
    const auto &scene = asset.scenes[active_scene_idx];

    for (size_t j = 0; j < scene.nodeIndices.size(); j++) {
      size_t node_idx = scene.nodeIndices[j];
      process_gltf_node(asset, file_entity, node_idx, file_entity, world);
    }
  }
}

void AssetImporter::import_gltf_model(const std::string &file_path,
                                      flecs::world &world) {

  std::filesystem::path path(file_path);

  if (!std::filesystem::exists(path)) {
    spdlog::error("Path not found: {}", file_path);
  }

  std::filesystem::path canon_path = std::filesystem::canonical(path);
  // Forces forward slashes on al OS's '/'
  std::string universal_path = canon_path.generic_string();

  // Deduplication
  std::string flecs_name = sanitize_name(universal_path, 0);
  flecs::entity file_entity = world.lookup(flecs_name.c_str());

  if (file_entity.is_valid()) {
    spdlog::info("Gltf file already loaded {}", file_path);
    return;
  }

  fastgltf::Parser parser;

  auto data = fastgltf::GltfDataBuffer::FromPath(canon_path);
  if (data.error() != fastgltf::Error::None) {
    spdlog::error("fastgltf failed to read raw file bytes: {}", universal_path);
    return;
  }

  auto asset_result =
      parser.loadGltf(data.get(), canon_path.parent_path(),
                      fastgltf::Options::GenerateMeshIndices |
                          fastgltf::Options::DecomposeNodeMatrices);

  if (asset_result.error() != fastgltf::Error::None) {
    spdlog::error("fastgltf failed to parse asset structural composition: {}");
    return;
  }

  fastgltf::Asset &asset = asset_result.get();
#ifndef NDEBUG

  auto validation_error = fastgltf::validate(asset);
  if (validation_error != fastgltf::Error::None) {

    spdlog::error("[DEBUG ONLY] fastgltf Spec Validation Error [{}]: File is "
                  "corrupted or violates glTF spec: {}",
                  fastgltf::getErrorMessage(validation_error), universal_path);
    return;
  }
  spdlog::info(
      "[DEBUG ONLY] Specification validation passed successfully for: {}",
      universal_path);

#endif
  // At this point file exists, is not a duplicate, and has completely passed
  // parsing/validation.
  file_entity =
      world.entity(flecs_name.c_str()).set<GltfFile>({universal_path, {}});

  // image importing
  import_gltf_images(asset, file_entity, world, canon_path);

  // material importing
  import_gltf_materials(asset, file_entity, world);

  // mesh importing
  import_gltf_meshes(asset, file_entity, world);

  // node processing
  process_gltf_scenes(asset, file_entity, world);
}

} // namespace Expectre
