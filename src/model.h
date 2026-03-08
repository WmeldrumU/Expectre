#ifndef EXPECTRE_MODEL_H
#define EXPECTRE_MODEL_H
#include "MathUtils.h"
#include "Mesh.h"
#include "ToolsVk.h"
#include "scene/SceneObject.h"
#include "Mesh.h"
#include "MeshManager.h"

#include <string>
#include <vector>
#include <vulkan/vulkan.h> // Make sure you include Vulkan header

namespace Expectre {

struct TextureInfo {
  uint32_t id;
  std::string type;
};

inline std::shared_ptr<SceneObject>
import_scene_object(const aiScene *scene, const aiNode *node, SceneObject &parent, const std::string &file_path = "") {
  if (node == nullptr) {
    return nullptr;
  }

  // convert ai node to sceneobject, separating out the meshes to their own SO's

  // Create primary scene object for this node
  std::shared_ptr<SceneObject> scene_object =
      std::make_shared<SceneObject>(&parent, node->mName.C_Str());

  scene_object->set_relative_transform(MathUtils::to_glm_4x3(node->mTransformation));

  // Create separate children nodes for each mesh.
  for (auto i = 0; i < node->mNumMeshes; i++) {
    auto mesh_index = node->mMeshes[i];
    auto ai_mesh = scene->mMeshes[mesh_index];

    std::shared_ptr<SceneObject> mesh_as_child_scene_object =
        std::make_shared<SceneObject>(scene_object.get(),
                                      ai_mesh->mName.C_Str());
    mesh_as_child_scene_object->set_relative_transform(
        glm::mat4x3(1.0f)); // child mesh transform will be same as parent

    MeshHandle mesh_handle = MeshManager::Instance().import_mesh(ai_mesh);
    // Set node mesh data
    mesh_as_child_scene_object->set_mesh(mesh_handle);

    scene_object->add_child(mesh_as_child_scene_object);
  }

  // Recurse for child nodes
  for (auto i = 0; i < node->mNumChildren; i++) {
    auto ai_child_node = node->mChildren[i];
    auto child = import_scene_object(scene, ai_child_node, *scene_object);
    if (child != nullptr) {
      scene_object->add_child(child);
    }
  }

  return scene_object;
}
struct Model {
  // glm::vec3 min_bounds{ 0.0f, 0.0f, 0.0f };
  // glm::vec3 max_bounds{ 0.0f, 0.0f, 0.0f };
  // glm::vec3 center{ 0.0f, 0.0f, 0.0f };
  // float radius{ 1.0f };

  static inline void import_model_as_scene_object(const std::string &file_path,
                                  SceneObject &import_parent) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(
        file_path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
      std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
      return;
    }

    const aiNode *root = scene->mRootNode;

    auto import_child = import_scene_object(scene, root, import_parent,  file_path);

    import_parent.add_child(import_child);
  }
};

} // namespace Expectre

#endif