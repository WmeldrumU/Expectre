#ifndef SCENE
#define SCENE

#include <filesystem>

#include "MaterialManager.h"
#include "MeshManager.h"
#include "input/InputManager.h"
#include "scene/Camera.h"
#include "scene/SceneObject.h"
#include "scene/SceneRoot.h"

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
  SceneObject &get_root() { return m_root; }
  Camera &get_camera() { return m_camera; }

private:
  void create_root();

  static inline std::shared_ptr<SceneObject>
  import_scene_object(const aiScene *scene, const aiNode *node,
                      SceneObject &parent, const std::string &file_path = "") {
    if (node == nullptr) {
      return nullptr;
    }
    // convert ai node to sceneobject, separating out the meshes to their own
    // SO's

    // Create primary scene object for this node
    std::shared_ptr<SceneObject> scene_object =
        std::make_shared<SceneObject>(&parent, node->mName.C_Str());

    scene_object->set_relative_transform(
        MathUtils::to_glm_4x3(node->mTransformation));

    // Create separate children nodes for each mesh.
    for (auto i = 0; i < node->mNumMeshes; i++) {
      auto mesh_index = node->mMeshes[i];
      auto ai_mesh = scene->mMeshes[mesh_index];

      std::shared_ptr<SceneObject> mesh_as_child_scene_object =
          std::make_shared<SceneObject>(scene_object.get(),
                                        ai_mesh->mName.C_Str());
      mesh_as_child_scene_object->set_relative_transform(
          glm::mat4x3(1.0f)); // child mesh transform will be same as parent

      // Import mesh
      MeshHandle mesh_handle = MeshManager::Instance().import_mesh(ai_mesh);

      // Import material if mesh has one, otherwise use default
      MaterialHandle material_handle;
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

      // Set mesh and material on scene object
      mesh_as_child_scene_object->set_mesh(mesh_handle);
      mesh_as_child_scene_object->set_material(material_handle);

      scene_object->add_child(mesh_as_child_scene_object);
    }

    // Recurse for child nodes
    for (auto i = 0; i < node->mNumChildren; i++) {
      auto ai_child_node = node->mChildren[i];
      auto child =
          import_scene_object(scene, ai_child_node, *scene_object, file_path);
      if (child != nullptr) {
        scene_object->add_child(child);
      }
    }

    return scene_object;
  }

  static inline void import_model_as_scene_object(const std::string &file_path,
                                                  SceneObject &import_parent) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(
        file_path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
      spdlog::error("ERROR::ASSIMP::{}", importer.GetErrorString());
      return;
    }

    const aiNode *root = scene->mRootNode;

    auto import_child =
        import_scene_object(scene, root, import_parent, file_path);

    import_parent.add_child(import_child);
  }

  SceneRoot m_root;
  Camera m_camera;
};
} // namespace Expectre
#endif // SCENE