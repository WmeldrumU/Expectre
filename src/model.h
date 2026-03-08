#ifndef EXPECTRE_MODEL_H
#define EXPECTRE_MODEL_H
#include <vector>
#include <string>
#include <vulkan/vulkan.h> // Make sure you include Vulkan header
#include "VkTools.h"
#include "VkTools.h"

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
		//std::vector<Vertex> vertices;
		//std::vector<uint32_t> indices;
		std::string name; // Name of the mesh
		uint32_t vertex_offset{ 0 }; // Offset in the vertex buffer
		uint32_t index_offset{ 0 };  // Offset in the index buffer
		// std::vector<Texture> textures; // Textures associated with the mesh
	};

	struct Model {
		std::vector<Mesh> meshes;
		glm::mat4 transform{ 1.0f }; // Identity matrix by default
		uint32_t vertex_count{ 0 };
		uint32_t index_count{ 0 };
		//std::vector<TextureInfo> textures;
		//glm::vec3 min_bounds{ 0.0f, 0.0f, 0.0f };
		//glm::vec3 max_bounds{ 0.0f, 0.0f, 0.0f };
		//glm::vec3 center{ 0.0f, 0.0f, 0.0f };
		//float radius{ 1.0f };

		static Model import_model(const std::string& file_path, std::vector<Expectre::Vertex>& vertices, std::vector<uint32_t>& indices)
		{
			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile(file_path,
				aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

			if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
			{
				std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
				return {};
			}

			const aiNode* root = scene->mRootNode;
			glm::mat4 model_transform = ToolsVk::to_glm(root->mTransformation);

			Expectre::Model model{};
			model.transform = model_transform;

			uint32_t running_vertex_offset = static_cast<uint32_t>(vertices.size());
			uint32_t running_index_offset = static_cast<uint32_t>(indices.size());

			// Iterate through each mesh
			for (unsigned int i = 0; i < scene->mNumMeshes; i++)
			{
				aiMesh* ai_mesh = scene->mMeshes[i];
				Expectre::Mesh mesh = {};

				mesh.vertex_offset = static_cast<uint32_t>(vertices.size());
				mesh.index_offset = static_cast<uint32_t>(indices.size());

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
						vertex.tex_coord.x = ai_mesh->mTextureCoords[0][j].x;
						vertex.tex_coord.y = ai_mesh->mTextureCoords[0][j].y;
					}
					else
					{
						vertex.tex_coord = glm::vec2(0.0f, 0.0f);
					}

					vertices.push_back(vertex);
				}

				// Indices
				for (auto j = 0; j < ai_mesh->mNumFaces; j++)
				{
					aiFace& face = ai_mesh->mFaces[j];
					for (auto k = 0; k < face.mNumIndices; k++)
					{
						indices.push_back(face.mIndices[k] + mesh.vertex_offset);
					}
				}
				mesh.index_offset = static_cast<uint32_t>(indices.size()) - mesh.index_offset;
				mesh.name = std::string(ai_mesh->mName.C_Str());
				model.meshes.push_back(mesh);
				model.vertex_count += static_cast<uint32_t>(vertices.size());
				model.index_count += static_cast<uint32_t>(indices.size());
			}
			return model;
		}

	};

}

#endif