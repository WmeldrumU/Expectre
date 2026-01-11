#ifndef EXPECTRE_MODEL_H
#define EXPECTRE_MODEL_H
#include <vector>
#include <string>
#include <vulkan/vulkan.h> // Make sure you include Vulkan header
#include "ToolsVk.h"
#include "MathUtils.h"

namespace Expectre
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec3 normal;
		glm::vec2 tex_coord;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription binding_description{};
			binding_description.binding = 0;
			binding_description.stride = sizeof(Vertex);
			binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return binding_description;
		}
	};

	struct TextureInfo
	{
		uint32_t id;
		std::string type;
	};

	struct Mesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::string name; // Name of the mesh
											// std::vector<Texture> textures; // Textures associated with the mesh
	};

	inline Mesh import_mesh(aiMesh *ai_mesh)
	{

		Mesh mesh{};
		mesh.name = ai_mesh->mName.C_Str();
		mesh.vertices.reserve(ai_mesh->mNumVertices);
		mesh.indices.reserve(ai_mesh->mNumFaces * 3);

		// Iterate through each vertex of the mesh
		for (unsigned int j = 0; j < ai_mesh->mNumVertices; j++)
		{
			Expectre::Vertex vertex;

			// Positions
			vertex.pos.x = ai_mesh->mVertices[j].x;
			vertex.pos.y = ai_mesh->mVertices[j].y;
			vertex.pos.z = ai_mesh->mVertices[j].z;

			// Normals
			if (ai_mesh->HasNormals())
			{
				vertex.normal.x = ai_mesh->mNormals[j].x;
				vertex.normal.y = ai_mesh->mNormals[j].y;
				vertex.normal.z = ai_mesh->mNormals[j].z;
			}

			// Texture Coordinates
			if (ai_mesh->mTextureCoords[0])
			{ // Check if the mesh contains texture coordinates
				vertex.tex_coord = {ai_mesh->mTextureCoords[0][j].x, ai_mesh->mTextureCoords[0][j].y};
			}
			else
			{
				vertex.tex_coord = glm::vec2(0.0f, 0.0f);
			}

			// set vertex color to black for now
			vertex.color = glm::vec3(0.0f);

			mesh.vertices.push_back(vertex);
		}

		// Indices
		for (auto j = 0; j < ai_mesh->mNumFaces; j++)
		{
			aiFace &face = ai_mesh->mFaces[j];
			for (auto k = 0; k < face.mNumIndices; k++)
			{
				mesh.indices.push_back(face.mIndices[k]);
			}
		}
		return mesh;
	}

	inline std::shared_ptr<SceneObject> process_node(const aiScene *scene, const aiNode *node, SceneObject &parent)
	{
		if (node == nullptr)
		{
			return nullptr;
		}

		// convert ai node to sceneobject, separating out the meshes to their own SO's

		// Create primary scene object for this node
		std::shared_ptr<SceneObject> scene_object = std::make_shared<SceneObject>(parent, node->mName.C_Str());

		scene_object->set_transform(MathUtils::to_glm_4x3(node->mTransformation));

		// Create separate children nodes for each mesh.
		for (auto i = 0; i < node->mNumMeshes; i++)
		{
			auto mesh_index = node->mMeshes[i];
			auto ai_mesh = scene->mMeshes[mesh_index];

			std::shared_ptr<SceneObject> mesh_as_child_scene_object = std::make_shared<SceneObject>(*scene_object, ai_mesh->mName.C_Str());
			mesh_as_child_scene_object->set_transform(glm::mat4x3(1.0f)); // child mesh transform will be same as parent

			Mesh imported_mesh = import_mesh(ai_mesh);
			// Set node mesh data
			mesh_as_child_scene_object->set_mesh(std::move(imported_mesh));

			scene_object->add_child(mesh_as_child_scene_object);
		}

		// Recurse for child nodes
		for (auto i = 0; i < node->mNumChildren; i++)
		{
			auto ai_child_node = node->mChildren[i];
			auto child = process_node(scene, ai_child_node, *scene_object);
			if (child != nullptr)
			{
				scene_object->add_child(child);
			}
		}

		return scene_object;
	}
	struct Model
	{
		// glm::vec3 min_bounds{ 0.0f, 0.0f, 0.0f };
		// glm::vec3 max_bounds{ 0.0f, 0.0f, 0.0f };
		// glm::vec3 center{ 0.0f, 0.0f, 0.0f };
		// float radius{ 1.0f };

		static inline void import_model(const std::string &file_path, SceneObject &import_parent)
		{
			Assimp::Importer importer;
			const aiScene *scene = importer.ReadFile(file_path,
																							 aiProcess_Triangulate |
																									 aiProcess_JoinIdenticalVertices);

			if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
			{
				std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
				return;
			}

			const aiNode *root = scene->mRootNode;

			auto import_child = process_node(scene, root, import_parent);

			import_parent.add_child(import_child);
		}
	};

}

#endif