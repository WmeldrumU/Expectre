#include "RenderContextVk.h"

#define VMA_IMPLEMENTATION
#define VMA_DEBUG_DETECT_LEAKS 1
#define VMA_STATS_STRING_ENABLED 1

#include "Engine.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "ToolsVk.h"
#include "scene/SceneObject.h"

#include <SDL3/SDL_vulkan.h> // <-- for SDL_Vulkan_CreateSurface
#include <bitset>
#include <cassert>
#include <spdlog/spdlog.h>
#include <vector>
#include <vma/vk_mem_alloc.h> // <-- for vmaCreateAllocator / vmaDestroyAllocator
#include <vulkan/vulkan.h>

namespace Expectre {

RenderContextVk::RenderContextVk(SDL_Window *window) : m_window{window} {
  // SDL_Vulkan_LoadLibrary();
  create_instance();
  create_surface();
  create_device();
  create_memory_allocator();

  m_renderer = std::make_shared<RendererVk>(
      m_physical_device, m_device, m_allocator, m_surface, m_graphics_queue,
      m_graphics_queue_index, m_present_queue, m_present_queue_index);
  m_ready = true;
}

RenderContextVk::~RenderContextVk() {

  // Destroy renderer first to free all its VMA allocations
  m_renderer.reset();

  vmaDestroyAllocator(m_allocator);

  // Destroy device, surface, instance
  vkDestroyDevice(m_device, nullptr);

  vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

  vkDestroyInstance(m_instance, nullptr);
}

void RenderContextVk::create_instance() {
  static std::vector<const char *> validation_layers = {
      "VK_LAYER_KHRONOS_validation"};

  auto instance_extensions =
      ToolsVk::get_required_instance_extensions(_is_debug_build);

  // Check for validation layer support
  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Expectre";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "No Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_4;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.enabledExtensionCount = instance_extensions.size();
  create_info.ppEnabledExtensionNames = instance_extensions.data();
  create_info.pApplicationInfo = &app_info;

#ifdef __APPLE__
  // This is the bit the error message is asking for
  create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};

  // Enable validation layers
  if (_is_debug_build) {
    ToolsVk::populate_debug_messenger_create_info(debug_create_info);
    create_info.enabledLayerCount = validation_layers.size();
    create_info.ppEnabledLayerNames = validation_layers.data();
    create_info.pNext = &debug_create_info;
  }

  VK_CHECK_RESULT(vkCreateInstance(&create_info, nullptr, &m_instance));
}

void RenderContextVk::create_device() {
  m_physical_device = ToolsVk::select_physical_device(m_instance);

  // Queue family logic
  uint32_t queue_families_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device,
                                           &queue_families_count, nullptr);
  assert(queue_families_count > 0);
  std::vector<VkQueueFamilyProperties> family_properties(queue_families_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
      m_physical_device, &queue_families_count, family_properties.data());

  // Check queues for present support
  std::vector<VkBool32> supports_present(queue_families_count);
  for (auto i = 0; i < queue_families_count; i++) {
    vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, m_surface,
                                         &supports_present.at(i));
  }

  // Search for queue that supports transfer, present, and graphics
  for (auto i = 0; i < queue_families_count; i++) {
    const auto &properties = family_properties.at(i);
    if (VK_QUEUE_TRANSFER_BIT & properties.queueFlags &&
        VK_QUEUE_GRAPHICS_BIT & properties.queueFlags &&
        supports_present.at(i)) {
      m_graphics_queue_index = i;
      m_present_queue_index = i;
      spdlog::debug("Choosing queue family with flags {} and count {}",
                    std::bitset<8>(properties.queueFlags).to_string(),
                    properties.queueCount);
      break;
    }
  }

  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.pNext = nullptr;
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.flags = 0;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &m_priority;
  queue_create_info.queueFamilyIndex = m_graphics_queue_index;

  VkPhysicalDeviceFeatures supportedFeatures{};
  vkGetPhysicalDeviceFeatures(m_physical_device, &supportedFeatures);
  VkPhysicalDeviceFeatures requiredFeatures{};
  requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
  requiredFeatures.tessellationShader = VK_TRUE;
  requiredFeatures.geometryShader = VK_TRUE;
  requiredFeatures.samplerAnisotropy = VK_TRUE;
  requiredFeatures.fillModeNonSolid = VK_TRUE;

  std::vector<const char *> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};

  VkDeviceCreateInfo device_create_info{};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pEnabledFeatures = &requiredFeatures;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledExtensionCount = extensions.size();
  device_create_info.ppEnabledExtensionNames = extensions.data();
  // Start creating logical device
  m_device = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateDevice(m_physical_device, &device_create_info,
                                 nullptr, &m_device));

  vkGetDeviceQueue(m_device, m_graphics_queue_index, 0, &m_graphics_queue);
  vkGetDeviceQueue(m_device, m_present_queue_index, 0, &m_present_queue);
}

void RenderContextVk::create_surface() {
  auto err =
      SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface);
  // Create a Vulkan surface using SDL
  if (!err) {
    const char *error = SDL_GetError();
    spdlog::error("SDL Vulkan surface creation failed: {}", error);
    // Handle surface creation error
    throw std::runtime_error("Failed to create Vulkan surface");
  }
}

void RenderContextVk::create_memory_allocator() {

  VmaAllocatorCreateInfo allocatorCreateInfo = {};
  allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
  allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;
  allocatorCreateInfo.physicalDevice = m_physical_device;
  allocatorCreateInfo.device = m_device;
  allocatorCreateInfo.instance = m_instance;

  vmaCreateAllocator(&allocatorCreateInfo, &m_allocator);
}

void RenderContextVk::UpdateAndRender(uint64_t delta_time, Scene &scene) {
  m_renderer->update(delta_time);
  m_renderer->draw_frame(scene.get_camera());
}
} // namespace Expectre