#ifndef SCENE_ASSET_IMPORTER_H
#define SCENE_ASSET_IMPORTER_H

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
#include "RenderableInfo.h"
#include "scene/TransformComponent.h"

namespace Expectre {

struct GltfFile {
  std::string universal_path;
  std::vector<flecs::entity> images;
  std::vector<flecs::entity> materials;
  std::vector<flecs::entity> meshes;
};

class AssetImporter {
public:
  // Import model at file path and append to entities and imported_renderables
  // Imported_renderables acts as a list draw calls for easy consumption when
  // rendering
  // void import_model_helper(const aiScene *scene, const aiNode *node,
  //                          const flecs::entity &parent,
  //                          const std::string &file_path, flecs::world
  //                          &world);
  std::string sanitize_name(const std::string &name, size_t index);
  void import_model(const std::string &file_path, flecs::world &world);

  void import_gltf_meshes(const fastgltf::Asset &asset,
                          flecs::entity &file_entity, flecs::world &world);

  void import_gltf_images(const fastgltf::Asset &asset,
                          flecs::entity &file_entity, flecs::world &world,
                          const std::filesystem::path &canonical_path);

  void import_gltf_materials(const fastgltf::Asset &asset,
                             flecs::entity &file_entity, flecs::world &world);

  void process_gltf_node(const fastgltf::Asset &asset,
                         flecs::entity file_entity, const size_t &node_idx,
                         const flecs::entity &parent, flecs::world &world);
  void process_gltf_scenes(const fastgltf::Asset &asset,
                           flecs::entity &file_entity, flecs::world &world);
  void import_gltf_model(const std::string &file_path, flecs::world &world);

private:
  fastgltf::Parser m_parser;
};

} // namespace Expectre

#endif // SCENE_ASSET_IMPORTER_H