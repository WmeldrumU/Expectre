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

    struct Model
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        // Possibly other model related data
    };
}

#endif