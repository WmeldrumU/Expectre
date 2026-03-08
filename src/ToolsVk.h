#ifndef VK_TOOLS_H
#define VK_TOOLS_H
/**
 * NOTE: many of the error checking tools are from Sascha Willem's Vulkan
 * repository (https://github.com/SaschaWillems/Vulkan)
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp> // Include this header for glm::pi<float>()
#include <glm/gtc/matrix_transform.hpp> // Add this include to ensure glm::perspective is available
#include <glm/mat4x3.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
// #define VK_NO_PROTOTYPES
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <spdlog/spdlog.h>

#include "Mesh.h"

#define VK_CHECK_RESULT(f)                                                     \
  {                                                                            \
    VkResult res = (f);                                                        \
    if (res != VK_SUCCESS) {                                                   \
      std::cout << "Fatal : VkResult is \""                                    \
                << ToolsVk::errorString(res).c_str() << "\" in " << __FILE__   \
                << " at line " << __LINE__ << "\n";                            \
      assert(res == VK_SUCCESS);                                               \
    }                                                                          \
  }

namespace Expectre {

// Vertex buffer and attributes
struct AllocatedBuffer {
  VmaAllocation allocation{VK_NULL_HANDLE}; // Allocation handle for the buffer
  VkBuffer buffer{}; // Handle to the Vulkan buffer object that the memory is
                     // bound to
  // Allow copying (optional but safe if you're careful)
  AllocatedBuffer() = default;
  AllocatedBuffer(const AllocatedBuffer &) = default;
  AllocatedBuffer &operator=(const AllocatedBuffer &) = default;
};
namespace ToolsVk {

static std::string errorString(VkResult errorCode) {
  switch (errorCode) {
#define STR(r)                                                                 \
  case VK_##r:                                                                 \
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

// This function is used to request a device memory type that supports all the
// property flags we request (e.g. device local, host visible) Upon success it
// will return the index of the memory type that fits our requested memory
// properties This is necessary as implementations can offer an arbitrary number
// of memory types with different memory properties. You can check
// https://vulkan.gpuinfo.org/ for details on different memory configurations
inline bool find_matching_memory(uint32_t type_bits, VkMemoryType *memory_types,
                                 VkFlags requirements, uint32_t *mem_index) {

  for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
    if (type_bits & 1) {
      // Type is available, now check if it meets our requirements
      if ((memory_types[i].propertyFlags & requirements) == requirements) {
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

static VkVertexInputBindingDescription getBindingDescription() {
  VkVertexInputBindingDescription binding_description{};
  binding_description.binding = 0;
  binding_description.stride = sizeof(Vertex);
  binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return binding_description;
}

static VkShaderModule createShaderModule(const VkDevice &device,
                                         const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
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
  if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create shader module!");
  }

  return shaderModule;
}

static VkExtent2D
choose_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities,
                   SDL_Window *window) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                               static_cast<uint32_t>(height)};

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

static VkSurfaceFormatKHR choose_swap_surface_format(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

static SwapChainSupportDetails
query_swap_chain_support(VkPhysicalDevice phys_device, VkSurfaceKHR surface) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &formatCount,
                                       nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface,
                                            &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.present_modes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        phys_device, surface, &presentModeCount, details.present_modes.data());
  }

  return details;
}

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
                                            VkSurfaceKHR surface) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }

    i++;
  }

  return indices;
}

static VkImageView create_image_view(VkDevice device, VkImage image,
                                     VkFormat format,
                                     VkImageAspectFlags aspectFlags) {
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
  if (vkCreateImageView(device, &view_info, nullptr, &imageView) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }

  return imageView;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data,
               void *p_user_data) {

  switch (message_severity) {
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

static void populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT &create_info) {
  create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = debug_callback;
}

static std::vector<const char *>
get_required_instance_extensions(bool enable_validation_layers) {
  uint32_t num_extensions = 0;

  auto raw_extensions = SDL_Vulkan_GetInstanceExtensions(&num_extensions);

  std::vector<const char *> extensions(raw_extensions,
                                       raw_extensions + num_extensions);

  if (enable_validation_layers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  // print extensions
  spdlog::debug("Extensions: \n");
  for (const char *ext : extensions) {
    spdlog::debug("- {}", ext);
  }

  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  return extensions;
}

static VkFormat find_supported_format(VkPhysicalDevice phys_device,
                                      const std::vector<VkFormat> &candidates,
                                      VkImageTiling tiling,
                                      VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(phys_device, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

static VkFormat find_depth_format(VkPhysicalDevice phys_device) {
  return find_supported_format(
      phys_device, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static VkCommandBuffer begin_single_time_commands(VkDevice device,
                                                  VkCommandPool command_pool) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = command_pool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

static void end_single_time_commands(VkDevice device, VkCommandPool cmd_pool,
                                     VkCommandBuffer cmd_buffer,
                                     VkQueue graphics_queue) {
  VK_CHECK_RESULT(vkEndCommandBuffer(cmd_buffer));

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buffer;

  vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphics_queue);

  vkFreeCommandBuffers(device, cmd_pool, 1, &cmd_buffer);
}

static AllocatedBuffer create_buffer(VmaAllocator allocator, VkDeviceSize size,
                                     VkBufferUsageFlags usage,
                                     VmaMemoryUsage memory_usage,
                                     VkMemoryPropertyFlags memory_properties,
                                     std::string label = "") {
  VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = memory_usage;
  alloc_info.requiredFlags = memory_properties;
  AllocatedBuffer result{};
  VK_CHECK_RESULT(vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                                  &result.buffer, &result.allocation, nullptr));
  return result;
}

static void copy_buffer(VkDevice device, VkCommandPool command_pool,
                        VkQueue graphics_queue, VkBuffer srcBuffer,
                        VkBuffer dstBuffer, VkBufferCopy copy_region) {
  VkCommandBuffer commandBuffer =
      ToolsVk::begin_single_time_commands(device, command_pool);


  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copy_region);

  ToolsVk::end_single_time_commands(device, command_pool, commandBuffer,
                                    graphics_queue);
}

static void copy_buffer_to_image(VkDevice device, VkCommandPool cmd_pool,
                                 VkQueue graphics_queue, VkBuffer buffer,
                                 VkImage image, uint32_t width,
                                 uint32_t height) {
  VkCommandBuffer command_buffer = begin_single_time_commands(device, cmd_pool);

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(command_buffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  end_single_time_commands(device, cmd_pool, command_buffer, graphics_queue);
}

static void set_object_name(VkDevice device, uint64_t objectHandle,
                            VkObjectType objectType, const char *name) {
  VkDebugUtilsObjectNameInfoEXT nameInfo = {};
  nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  nameInfo.objectType = objectType;
  nameInfo.objectHandle = objectHandle;
  nameInfo.pObjectName = name;

  // Function pointer must be loaded at runtime:
  auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
      device, "vkSetDebugUtilsObjectNameEXT");

  if (func) {
    func(device, &nameInfo);
  }
}

static VkSampler create_texture_sampler(VkPhysicalDevice physical_device,
                                        VkDevice device) {
  VkSampler sampler;

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.anisotropyEnable = VK_TRUE;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE; // Sample with [0, 1] range
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.mipLodBias = 0.0f;
  sampler_info.minLod = 0.0f;
  sampler_info.maxLod = 0.0f;

  VK_CHECK_RESULT(vkCreateSampler(device, &sampler_info, nullptr, &sampler));
  return sampler;
}

static void DebugMarker(VkCommandBuffer cmd_buffer, std::string name) {
  VkDebugMarkerMarkerInfoEXT info{};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
  info.pMarkerName = name.c_str();
  vkCmdDebugMarkerBeginEXT(cmd_buffer, &info);
}

static VkPhysicalDevice select_physical_device(VkInstance instance) {

  uint32_t gpu_count = 0;
  std::vector<VkPhysicalDevice> gpus;
  vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
  gpus.resize(gpu_count);
  vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());

  if (gpu_count == 0) {
    spdlog::error("Could not find a physical device");
    throw std::runtime_error("Could not find a physical device");
  }

  VkPhysicalDevice chosen_phys_device = VK_NULL_HANDLE;
  VkDeviceSize max_vram = 0;

  for (auto &gpu : gpus) {
    VkPhysicalDeviceProperties phys_properties{};
    vkGetPhysicalDeviceProperties(gpu, &phys_properties);
    std::cout << "Physical device: " << std::endl;
    std::cout << phys_properties.deviceName << std::endl;
    std::cout << "API VERSION: "
              << VK_API_VERSION_MAJOR(phys_properties.apiVersion) << "."
              << VK_API_VERSION_MINOR(phys_properties.apiVersion) << std::endl;
    std::cout << "device type: " << phys_properties.deviceType << std::endl;

    VkPhysicalDeviceFeatures phys_features{};
    vkGetPhysicalDeviceFeatures(gpu, &phys_features);

    VkPhysicalDeviceMemoryProperties phys_memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &phys_memory_properties);
    std::cout << "heap count: " << phys_memory_properties.memoryHeapCount
              << std::endl;

    // Calculate total VRAM (device local memory)
    VkDeviceSize total_vram = 0;
    for (uint32_t j = 0; j < phys_memory_properties.memoryHeapCount; j++) {
      VkMemoryHeap heap = phys_memory_properties.memoryHeaps[j];
      std::cout << "heap size:" << std::endl;
      std::cout << heap.size << std::endl;
      std::cout << "flags: " << heap.flags << std::endl;

      // Only count device local memory (VRAM)
      if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
        total_vram += heap.size;
      }
    }

    std::cout << "Total VRAM: " << (total_vram / (1024 * 1024)) << " MB"
              << std::endl;

    // Prioritize discrete GPUs first, then choose device with most VRAM
    bool is_better = false;
    if (chosen_phys_device == VK_NULL_HANDLE) {
      is_better = true;
    } else {
      VkPhysicalDeviceProperties current_properties{};
      vkGetPhysicalDeviceProperties(chosen_phys_device, &current_properties);

      bool current_is_discrete =
          current_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
      bool new_is_discrete =
          phys_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

      // Prioritize discrete GPUs
      if (new_is_discrete && !current_is_discrete) {
        is_better = true;
      }
      // If both discrete or both not discrete, choose by VRAM
      else if (new_is_discrete == current_is_discrete &&
               total_vram > max_vram) {
        is_better = true;
      }
    }

    if (is_better) {
      chosen_phys_device = gpu;
      max_vram = total_vram;
      spdlog::info("Selected physical device: {} ({}) with {} MB VRAM",
                   phys_properties.deviceName,
                   phys_properties.deviceType ==
                           VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                       ? "Discrete"
                       : "Integrated",
                   max_vram / (1024 * 1024));
    }
  }

  return chosen_phys_device;
}

} // namespace ToolsVk
} // namespace Expectre

#endif // VK_TOOLS_H