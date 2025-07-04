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
// #define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "Model.h"
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

	glm::mat4 to_glm(const aiMatrix4x4 &m)
	{
		return glm::mat4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4);
	}

	Expectre::Model import_model(const std::string &file_path, std::vector<Expectre::Vertex> &vertices, std::vector<uint32_t> &indices)
	{
		Assimp::Importer importer;
		const aiScene *scene = importer.ReadFile(file_path,
												 aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
			return {};
		}

		const aiNode *root = scene->mRootNode;
		glm::mat4 model_transform = to_glm(root->mTransformation);

		Expectre::Model model{
			.transform = model_transform};

		uint32_t running_vertex_offset = static_cast<uint32_t>(vertices.size());
		uint32_t running_index_offset = static_cast<uint32_t>(indices.size());

		// Iterate through each mesh
		for (unsigned int i = 0; i < scene->mNumMeshes; i++)
		{
			aiMesh *ai_mesh = scene->mMeshes[i];
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
				aiFace &face = ai_mesh->mFaces[j];
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

	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities, SDL_Window *window)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			SDL_GetWindowSizeInPixels(window, &width, &height);
			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR> &availableFormats)
	{
		for (const auto &availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice phys_device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.present_modes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &presentModeCount, details.present_modes.data());
		}

		return details;
	}

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto &queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			i++;
		}

		return indices;
	}

	VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo view_info{};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = image;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = format;
		view_info.subresourceRange.aspectMask = aspectFlags;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &view_info, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture image view!");
		}

		return imageView;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data, void *p_user_data)
	{

		switch (message_severity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			spdlog::info("VERBOSE: {}", p_callback_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			spdlog::info("INFO: {}", p_callback_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			spdlog::warn("WARNING: {}", p_callback_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			spdlog::error("ERROR: {}", p_callback_data->pMessage);
			// return VK_FALSE;
			break;
		default:
			spdlog::info("UNKNOWN: {}", p_callback_data->pMessage);
			break;
		}
		return VK_FALSE;
	}

	void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT &create_info)
	{
		create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		create_info.pfnUserCallback = debug_callback;
	}

	std::vector<const char *> get_required_instance_extensions(bool enable_validation_layers)
	{
		uint32_t num_extensions = 0;

		auto raw_extensions = SDL_Vulkan_GetInstanceExtensions(&num_extensions);

		std::vector<const char *> extensions(raw_extensions, raw_extensions + num_extensions);

		if (enable_validation_layers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		// print extensions
		for (const char *ext : extensions)
		{
			std::printf("%s", ext);
			std::cout << "\n"
					  << std::endl;
		}

		return extensions;
	}

	VkFormat find_supported_format(VkPhysicalDevice phys_device, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(phys_device, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	VkFormat find_depth_format(VkPhysicalDevice phys_device)
	{
		return find_supported_format(phys_device,
									 {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
									 VK_IMAGE_TILING_OPTIMAL,
									 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

} // namespace tools