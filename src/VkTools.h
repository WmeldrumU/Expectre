/**
 * NOTE: many of the error checking tools are from Sascha Willem's Vulkan
 * repository (https://github.com/SaschaWillems/Vulkan)
 */

#include <string>
#include <fstream>
#include <vector>
#include <optional>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "model.h"
#define VK_CHECK_RESULT(f)                                                                                                                       \
    {                                                                                                                                            \
        VkResult res = (f);                                                                                                                      \
        if (res != VK_SUCCESS)                                                                                                                   \
        {                                                                                                                                        \
            std::cout << "Fatal : VkResult is \"" << tools::errorString(res).c_str() << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
            assert(res == VK_SUCCESS);                                                                                                           \
        }                                                                                                                                        \
    }
namespace tools
{

    std::string errorString(VkResult errorCode)
    {
        switch (errorCode)
        {
#define STR(r)   \
    case VK_##r: \
        return #r
            STR(NOT_READY);
            STR(TIMEOUT);
            STR(EVENT_SET);
            STR(EVENT_RESET);
            STR(INCOMPLETE);
            STR(ERROR_OUT_OF_HOST_MEMORY);
            STR(ERROR_OUT_OF_DEVICE_MEMORY);
            STR(ERROR_INITIALIZATION_FAILED);
            STR(ERROR_DEVICE_LOST);
            STR(ERROR_MEMORY_MAP_FAILED);
            STR(ERROR_LAYER_NOT_PRESENT);
            STR(ERROR_EXTENSION_NOT_PRESENT);
            STR(ERROR_FEATURE_NOT_PRESENT);
            STR(ERROR_INCOMPATIBLE_DRIVER);
            STR(ERROR_TOO_MANY_OBJECTS);
            STR(ERROR_FORMAT_NOT_SUPPORTED);
            STR(ERROR_SURFACE_LOST_KHR);
            STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
            STR(SUBOPTIMAL_KHR);
            STR(ERROR_OUT_OF_DATE_KHR);
            STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
            STR(ERROR_VALIDATION_FAILED_EXT);
            STR(ERROR_INVALID_SHADER_NV);
            STR(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#undef STR
        default:
            return "UNKNOWN_ERROR";
        }
    }

    // This function is used to request a device memory type that supports all the property flags we request (e.g. device local, host visible)
    // Upon success it will return the index of the memory type that fits our requested memory properties
    // This is necessary as implementations can offer an arbitrary number of memory types with different
    // memory properties.
    // You can check https://vulkan.gpuinfo.org/ for details on different memory configurations
    bool find_matching_memory(uint32_t type_bits, VkMemoryType *memory_types,
                              VkFlags requirements, uint32_t *mem_index)
    {

        for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
        {
            if (type_bits & 1)
            {
                // Type is available, now check if it meets our requirements
                if ((memory_types[i].propertyFlags & requirements) == requirements)
                {
                    *mem_index = i;
                    return true;
                }
            }
            // Shift bits down
            type_bits >>= 1;
        }
        // No matching memory types
        return false;
    }

    VkShaderModule createShaderModule(const VkDevice &device, const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buffer.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module!");
        }

        return shaderModule;
    }

    void printMatrix(const glm::mat4 &matrix)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                std::cout << matrix[j][i] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    Expectre::Model import_model(const std::string &file_path)
    {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(file_path,
                                                 aiProcess_Triangulate | aiProcess_JoinIdenticalVertices) ;

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
            return {};
        }

        std::vector<Expectre::Vertex> vertices;
        std::vector<uint32_t> indices;

        // Iterate through each mesh
        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[i];

            // Iterate through each vertex of the mesh
            for (unsigned int j = 0; j < mesh->mNumVertices; j++)
            {
                Expectre::Vertex vertex;

                // Positions
                vertex.pos.x = mesh->mVertices[j].x;
                vertex.pos.y = mesh->mVertices[j].y;
                vertex.pos.z = mesh->mVertices[j].z;

                // Normals
                if (mesh->HasNormals())
                {
                    vertex.normal.x = mesh->mNormals[j].x;
                    vertex.normal.y = mesh->mNormals[j].y;
                    vertex.normal.z = mesh->mNormals[j].z;
                }

                // Texture Coordinates
                if (mesh->mTextureCoords[0])
                { // Check if the mesh contains texture coordinates
                    vertex.tex_coord.x = mesh->mTextureCoords[0][j].x;
                    vertex.tex_coord.y = mesh->mTextureCoords[0][j].y;
                }
                else
                {
                    vertex.tex_coord = glm::vec2(0.0f, 0.0f);
                }

                vertices.push_back(vertex);
            }

            // Iterate through faces
            for (auto j = 0; j < mesh->mNumFaces; j++)
            {
                aiFace &face = mesh->mFaces[j];
                for (auto k = 0; k < face.mNumIndices; k++)
                {
                    indices.push_back(face.mIndices[k]);
                }
            }
        }

        Expectre::Model model{};
        model.indices = indices;
        model.vertices = vertices;

        return model;
    }

}