#ifndef EXPECTRE_MODEL_H
#define EXPECTRE_MODEL_H
#include <vector>
#include <string>
#include <vulkan/vulkan.h> // Make sure you include Vulkan header
#include <glm/vec3.hpp>    // For glm::vec3
#include <glm/vec2.hpp>    // For glm::vec2

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
	};

}

#endif