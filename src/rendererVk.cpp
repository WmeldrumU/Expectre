
#include <glm/glm.hpp>
#include <iostream>
#include <set>

#include "rendererVk.h"
#include "shared.h"

struct
{
    glm::mat4 modelMatrix;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
} uboVS;

namespace Expectre
{
    Renderer_Vk::Renderer_Vk()
    {

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        {
            SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
            throw std::runtime_error("failed to initialize SDL!");
        }

        if (SDL_Vulkan_LoadLibrary(NULL))
        {
            SDL_Log("Unable to initialize vulkan lib: %s", SDL_GetError());
            throw std::runtime_error("SDL failed to load Vulkan Library!");
        }
        m_window = SDL_CreateWindow("WINDOW TITLE",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    1280, 720,
                                    SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

        if (!m_window)
        {
            SDL_Log("Unable to initialize application window!: %s", SDL_GetError());
            throw std::runtime_error("Unable to initialize application window!");
        }

        create_instance();

        // create_debug();

        // create_surface();

        // cnumeratePhysicalDevices();

        select_physical_device();

        // select_queue_family();

        // screate_logical_device_and_queues(); //problem function

        // createSemaphores();

        // createCommandPool();

        // allocator.init();

        // stagingManager.Init();

        // CreateSwapChain();

        // CreateRenderTargets();

        // createRenderPass();

        // createPipelineCache();

        // createFrameBuffers();

        // renderProgManager.Init();

        // vertexCache.Init(... );
    }

    Renderer_Vk::~Renderer_Vk()
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    void Renderer_Vk::create_instance()
    {

        uint32_t num_extensions = 0;

        if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &num_extensions, nullptr))
        {
            SDL_Log("Unable to get number of extensions: %s", SDL_GetError());
            throw std::runtime_error("Unable to get number of extensions");
        }
        std::vector<const char *> extensions(num_extensions);
        if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &num_extensions, extensions.data()))
        {
            SDL_Log("Unable to get vulkan extensions: %s", SDL_GetError());
            throw std::runtime_error("Unable to get vulkan extension names:");
        }

        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Expectre App";
        app_info.applicationVersion = 1;
        app_info.apiVersion = VK_API_VERSION_1_3; // Specify what version of vulkan we want

        VkInstanceCreateInfo instance_create_info{};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pNext = nullptr;
        instance_create_info.pApplicationInfo = &app_info; // create_info depends on app_info
        instance_create_info.enabledLayerCount = 0;

        vkCreateInstance(&instance_create_info, nullptr, &m_instance);
    }

    // void Renderer_Vk::create_debug() {
    //     PFN_vkCreateDebugReportCallbackEXT SDL2_vkCreateDebugReportCallbackEXT = nullptr;
    //     VkDebugReportCallbackCreateInfoEXT debug_callback_create_info {};
    //     debug_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    //     debug_callback_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIG_EXT;
    //     debug_callback_create_info.pfnCallback = //some function
    // }

    void Renderer_Vk::create_surface()
    {
        SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface);
    }

    void Renderer_Vk::select_physical_device()
    {
        uint32_t physical_device_count = 0;
        vkEnumeratePhysicalDevices(m_instance, &physical_device_count, nullptr);
        if (physical_device_count == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        m_physical_devices.resize(physical_device_count);
        vkEnumeratePhysicalDevices(m_instance, &physical_device_count, m_physical_devices.data());

        for (uint32_t i = 0; i < m_physical_devices.size(); i++)
        {
            const VkPhysicalDevice &phys_device = m_physical_devices.at(i);
            VkPhysicalDeviceProperties phys_properties{};
            vkGetPhysicalDeviceProperties(phys_device, &phys_properties);
            std::cout << "Physical device: " << std::endl;
            std::cout << phys_properties.deviceName << std::endl;
            std::cout << "API VERSION: " << VK_API_VERSION_MAJOR(phys_properties.apiVersion) << "." << VK_API_VERSION_MINOR(phys_properties.apiVersion) << std::endl;
            std::cout << "device type: " << phys_properties.deviceType << std::endl;
            VkPhysicalDeviceFeatures phys_features{};
            vkGetPhysicalDeviceFeatures(phys_device, &phys_features);

            VkPhysicalDeviceMemoryProperties phys_memory_properties{};
            vkGetPhysicalDeviceMemoryProperties(phys_device, &phys_memory_properties);
            std::cout << "heap count: " << phys_memory_properties.memoryHeapCount << std::endl;
            for (uint32_t j = 0; j < phys_memory_properties.memoryHeapCount; j++)
            {
                VkMemoryHeap heap{};
                heap = phys_memory_properties.memoryHeaps[j];
                // if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == false)
                // {
                //     continue;
                // }

                std::cout << "heap size:" << std::endl;
                std::cout << heap.size << std::endl;
                std::cout << "flags: " << heap.flags << std::endl;
            }

            if (phys_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
                phys_features.geometryShader)
            {
                m_chosen_phys_device = phys_device;
                std::cout << "chose phys device: " << phys_properties.deviceName << std::endl;
            }
        }
    }

    void Renderer_Vk::select_queue_family()
    {

        uint32_t queue_family_count;

        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_devices[0], &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_devices[0], &queue_family_count, queue_family_properties.data());

        int32_t graphic_index = -1;
        int32_t present_index = -1;

        int32_t i = 0;
        for (const auto &queue_family : queue_family_properties)
        {
            if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphic_index = i;
            }

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_devices[0], i, m_surface, &present_support);
            if (queue_family.queueCount > 0 && present_support)
            {
                present_index = i;
            }

            if (graphic_index != -1 && present_index != -1)
            {
                break; // found queue(s) for graphics and present functionality
            }

            i++;
        }
    }

    void Renderer_Vk::create_logical_device_and_queues()
    {
        std::vector<const char *> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        float queue_priority[] = {1.0f};

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos{};
        std::set<uint32_t> unique_queue_families = {graphics_queue_family_index, present_queue_family_index};

        float float_queue_priority = queue_priority[0];
        for (uint32_t queue_family : unique_queue_families)
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &float_queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        // Graphics Queue
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = graphics_queue_family_index;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &float_queue_priority;

        // device feature support
        VkPhysicalDeviceFeatures device_features{};
        device_features.samplerAnisotropy = VK_TRUE; // Anisotropic filtering

        // logical device
        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.queueCreateInfoCount = queue_create_infos.size();
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.pEnabledFeatures = &device_features;
        device_create_info.enabledExtensionCount = device_extensions.size();
        device_create_info.ppEnabledExtensionNames = device_extensions.data();

        device_create_info.enabledLayerCount = m_validation_layers.size();
        device_create_info.ppEnabledLayerNames = m_validation_layers.data();

        std::cout << queue_create_infos.size() << std::endl;

        // for (auto info: queue_create_infos) {
        //     std::cout << info. << std::endl;
        // }
        // TODO: change physical device picking logic!!
        // TODO: check if index should be 0 or 1
        vkCreateDevice(m_physical_devices[0], &device_create_info, nullptr, &m_device);

        vkGetDeviceQueue(m_device, graphics_queue_family_index, 0, &graphics_queue);
        vkGetDeviceQueue(m_device, present_queue_family_index, 0, &present_queue);
    }
    void Renderer_Vk::cleanup()
    {
    }
}