
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <set>
#include <bitset>
#include <assert.h>
#include <vulkan/vulkan.h>
#include <array>

#include "rendererVk.h"
#include "shared.h"
#include "spdlog/spdlog.h"
#include "VkTools.h"

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

        enumerate_layers();

        create_instance();

        create_surface();

        select_physical_device();

        query_swap_chain_support();

        create_logical_device_and_queues();

        get_surface_format();

        create_swap_chain();

        create_command_buffers();

        prepare_depth();

        create_vertex_buffer();

        create_layouts();

        prepare_render_pass();

        prepare_pipeline();

        // create_buffers_and_images();

        // create_views();

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
        // cleanup();
    }

    Renderer_Vk::~Renderer_Vk()
    {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

        // Destroy depth buffer
        vkDestroyImageView(m_device, m_depth.view, nullptr);
        vkDestroyImage(m_device, m_depth.image, nullptr);
        vkFreeMemory(m_device, m_depth.mem, nullptr);

        // Destroy uniform buffers
        for (auto i = 0; i < m_swapchain_buffers.size(); i++)
        {
            vkDestroyImageView(m_device, m_swapchain_buffers[i].view, nullptr);
            vkDestroyBuffer(m_device, m_swapchain_uniform_buffers[i], nullptr);
            vkFreeMemory(m_device, m_uniform_memories[i], nullptr);
        }
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDeviceWaitIdle(m_device);
        vkDestroyImageView(m_device, m_image_view, nullptr);
        vkDestroyImage(m_device, m_image, nullptr);
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        vkDestroyDevice(m_device, nullptr);
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

        // print extensions
        for (auto ext : extensions)
        {
            std::printf(ext);
            std::cout << "\n"
                      << std::endl;
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

        uint32_t layer_count = 0;
        const char **p_layers = nullptr;
        if (m_layers_supported)
        {
            layer_count = static_cast<uint32_t>(m_layers.size());
            p_layers = m_layers.data();
        }
        instance_create_info.enabledLayerCount = layer_count;
        instance_create_info.ppEnabledLayerNames = p_layers;

        // Extensions
        instance_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instance_create_info.ppEnabledExtensionNames = &extensions.at(0);

        auto result = vkCreateInstance(&instance_create_info, nullptr, &m_instance);

        if (result != VK_SUCCESS)
        {
            spdlog::error("Could not create instance.");
        }
    }

    void Renderer_Vk::enumerate_layers()
    {
        VkResult err;
        uint32_t count = 0;
        err = vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> properties(count);
        if (count == 0)
        {
            return;
        }

        err = vkEnumerateInstanceLayerProperties(&count, properties.data());
        assert(!err);

        for (const auto &property : properties)
        {
            for (const auto &layer_name : m_layers)
            {
                if (strcmp(property.layerName, layer_name) == 0)
                {
                    m_layers_supported = true;
                    spdlog::debug("Validation layers found!");
                    return;
                }
            }

            // spdlog::debug(property.layerName);
            // spdlog::debug(property.description);
        }
        if (!m_layers_supported)
        {
            spdlog::debug("Validation layers NOT found!");
        }
    }

    void Renderer_Vk::create_surface()
    {
        auto err = SDL_Vulkan_CreateSurface(m_window, m_instance, static_cast<SDL_vulkanSurface *>(&m_surface));
        // Create a Vulkan surface using SDL
        if (!err)
        {
            // Handle surface creation error
            throw std::runtime_error("Failed to create Vulkan surface");
        }
    }

    void Renderer_Vk::select_physical_device()
    {
        // Enumerate physical devices
        uint32_t physical_device_count = 0;
        auto err = vkEnumeratePhysicalDevices(m_instance, &physical_device_count, nullptr);
        if (physical_device_count <= 0)
        {
            std::runtime_error("<= 0 physical devices available");
        }
        assert(!err);
        m_physical_devices.resize(physical_device_count);
        vkEnumeratePhysicalDevices(m_instance, &physical_device_count, m_physical_devices.data());

        // Report available devices
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

            vkGetPhysicalDeviceMemoryProperties(phys_device, &m_phys_memory_properties);
            std::cout << "heap count: " << m_phys_memory_properties.memoryHeapCount << std::endl;
            for (uint32_t j = 0; j < m_phys_memory_properties.memoryHeapCount; j++)
            {
                VkMemoryHeap heap{};
                heap = m_phys_memory_properties.memoryHeaps[j];

                std::cout << "heap size:" << std::endl;
                std::cout << heap.size << std::endl;
                std::cout << "flags: " << heap.flags << std::endl;
            }
            if (!phys_features.geometryShader)
            {
                continue;
            }

            if (phys_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                !m_chosen_phys_device.has_value())
            {
                m_chosen_phys_device = phys_device;
                spdlog::debug("chose physical device: " + std::string(phys_properties.deviceName));
            }
        }

        assert(m_chosen_phys_device.has_value());
    }

    void Renderer_Vk::create_logical_device_and_queues()
    {
        // Queue family logic
        uint32_t queue_families_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_chosen_phys_device.value(),
                                                 &queue_families_count,
                                                 nullptr);
        assert(queue_families_count > 0);
        std::vector<VkQueueFamilyProperties> family_properties(queue_families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_chosen_phys_device.value(),
                                                 &queue_families_count,
                                                 family_properties.data());

        // Check queues for present support
        std::vector<VkBool32> supports_present(queue_families_count);
        for (auto i = 0; i < queue_families_count; i++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(m_chosen_phys_device.value(), i,
                                                 m_surface, &supports_present.at(i));
        }

        // Search for queue that supports transfer, present, and graphics
        auto family_index = 0;
        for (auto i = 0; i < queue_families_count; i++)
        {
            const auto &properties = family_properties.at(i);
            if (VK_QUEUE_TRANSFER_BIT & properties.queueFlags &&
                VK_QUEUE_GRAPHICS_BIT & properties.queueFlags &&
                supports_present.at(i))
            {
                family_index = i;
                m_graphics_queue_family_index = i;
                m_present_queue_family_index = i;
                spdlog::debug("Choosing queue family with flags {} and count {}",
                              std::bitset<8>(properties.queueFlags).to_string(), properties.queueCount);
                break;
            }
        }

        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.pNext = nullptr;
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.flags = NULL;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &m_priority;
        queue_create_info.queueFamilyIndex = family_index;

        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(m_chosen_phys_device.value(), &supportedFeatures);
        VkPhysicalDeviceFeatures requiredFeatures{};
        requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
        requiredFeatures.tessellationShader = VK_TRUE;
        requiredFeatures.geometryShader = VK_TRUE;

        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pEnabledFeatures = &requiredFeatures;
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.enabledExtensionCount = m_required_device_extensions.size();
        device_create_info.ppEnabledExtensionNames = m_required_device_extensions.data();
        // Start creating logical device
        if (vkCreateDevice(m_chosen_phys_device.value(), &device_create_info, nullptr, &m_device) != VK_SUCCESS)
        {
            spdlog::throw_spdlog_ex("Could not create logical device");
        }

        vkGetDeviceQueue(m_device, family_index, 0, &m_graphics_queue);
    }

    void Renderer_Vk::get_surface_format()
    {
        uint32_t format_count = 0;
        VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_chosen_phys_device.value(),
            m_surface, &format_count, nullptr);
        assert(!err);
        std::vector<VkSurfaceFormatKHR> surf_formats(format_count);
        err = vkGetPhysicalDeviceSurfaceFormatsKHR(
            m_chosen_phys_device.value(), m_surface,
            &format_count, surf_formats.data());
        assert(!err);

        for (auto i = 0; i < format_count; i++)
        {
            VkFormat format = surf_formats[i].format;
            if (format == VK_FORMAT_R8G8B8A8_UNORM ||
                format == VK_FORMAT_B8G8R8A8_UNORM ||
                format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
                format == VK_FORMAT_A1R5G5B5_UNORM_PACK16)
            {
                m_surface_format = surf_formats[i];
                return;
            }
        }

        assert(format_count >= 1);

        spdlog::warn("Cannot find preferred format, assigning first found");
        m_surface_format = surf_formats[0];
    }

    void Renderer_Vk::create_buffers_and_images()
    {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = 1280 * 720 * 512; // 512MB
        buffer_info.pNext = nullptr;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                            VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device, &buffer_info, nullptr, &m_buffer) != VK_SUCCESS)
        {
            spdlog::throw_spdlog_ex("Could not create buffer");
        }

        VkFormat format{};
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_chosen_phys_device.value(), format, &properties);

        // vkGetPhysicalDeviceImageFormatProperties(m_device, )

        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_info.extent = {1280, 720, 1};
        image_info.mipLevels = 10;
        image_info.arrayLayers = 1;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device, &image_info, nullptr, &m_image) != VK_SUCCESS)
        {
            spdlog::throw_spdlog_ex("Could not create image.");
        }
    }

    void Renderer_Vk::create_views()
    {

        // VkBufferViewCreateInfo buffer_view_info{};
        // buffer_view_info.buffer = m_buffer;
        // buffer_view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        // buffer_view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        // buffer_view_info.offset = ;
        // buffer_view_info.range = ;

        // TODO: needs swapchaini info first
        VkComponentMapping mapping{}; // mapping is automatically set to VK_COMPONENT_SWIZZLE_IDENTITY
        VkImageViewCreateInfo image_view_info{};
        image_view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.components = mapping;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;

        // mapping.r =
        // image_view_info.components.r
    }

    uint32_t Renderer_Vk::choose_heap_from_flags(const VkMemoryRequirements &memoryRequirements,
                                                 VkMemoryPropertyFlags requiredFlags,
                                                 VkMemoryPropertyFlags preferredFlags)
    {
        VkPhysicalDeviceMemoryProperties device_memory_properties;

        vkGetPhysicalDeviceMemoryProperties(m_chosen_phys_device.value(), &device_memory_properties);

        uint32_t selected_type = ~0u; // All 1's
        uint32_t memory_type;

        for (memory_type = 0; memory_type < 32; memory_type++)
        {

            if (memoryRequirements.memoryTypeBits & (1 << memory_type))
            {
                const VkMemoryType &type = device_memory_properties.memoryTypes[memory_type];

                // If type has all our preferred flags
                if ((type.propertyFlags & preferredFlags) == preferredFlags)
                {
                    selected_type = memory_type;
                    break;
                }
            }
        }

        if (selected_type != ~0u)
        {
            for (memory_type = 0; memory_type < 32; memory_type++)
            {

                if (memoryRequirements.memoryTypeBits & (1 << memory_type))
                {
                    const VkMemoryType &type = device_memory_properties.memoryTypes[memory_type];

                    // If type has all our required flags
                    if ((type.propertyFlags & requiredFlags) == requiredFlags)
                    {
                        selected_type = memory_type;
                        break;
                    }
                }
            }
        }
        return selected_type;
    }

    void Renderer_Vk::query_swap_chain_support()
    {
        VkResult err;
        uint32_t device_extension_count = 0;
        // Get number of extensions
        err = vkEnumerateDeviceExtensionProperties(
            m_chosen_phys_device.value(),
            nullptr, &device_extension_count, nullptr);
        assert(!err);
        // If extensions exist
        if (device_extension_count > 0)
        {
            // Get device extensions
            std::vector<VkExtensionProperties> device_extensions(device_extension_count);
            err = vkEnumerateDeviceExtensionProperties(
                m_chosen_phys_device.value(), nullptr,
                &device_extension_count, device_extensions.data());
            // Make sure required device extensions exist
            std::set<std::string> required(m_required_device_extensions.begin(),
                                           m_required_device_extensions.end());
            for (const auto &extension : device_extensions)
            {
                required.erase(extension.extensionName);
            }

            if (!required.empty())
            {
                std::runtime_error("Not all required device extensions found");
            }
        }
        else
        {
            std::runtime_error("No device extensions found");
        }
    }

    void Renderer_Vk::create_semaphors_and_fences()
    {
        VkSemaphoreCreateInfo sem_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
    }

    void Renderer_Vk::create_swap_chain()
    {
        VkResult err;
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            m_chosen_phys_device.value(), m_surface, &capabilities);
        spdlog::debug("capabilities.maxImageCount: {}", capabilities.maxImageCount);
        VkSwapchainCreateInfoKHR swapchain_ci{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .surface = m_surface,
            .minImageCount = capabilities.maxImageCount,
            .imageFormat = m_surface_format.format,
            .imageColorSpace = m_surface_format.colorSpace,

            .imageExtent = {
                .width = 1280,
                .height = 720,
            },
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,

            .clipped = true,
            .oldSwapchain = VK_NULL_HANDLE,
        };
        err = vkCreateSwapchainKHR(m_device, &swapchain_ci, nullptr, &m_swapchain);
        VK_CHECK_RESULT(err);

        uint32_t swapchain_image_count = 0;
        err = vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchain_image_count, NULL);
        VK_CHECK_RESULT(err);

        // Resize swapchain image resoruce vectors
        m_swapchain_uniform_buffers.resize(swapchain_image_count);
        m_uniform_memories.resize(swapchain_image_count);
        m_swapchain_images.resize(swapchain_image_count);

        err = vkGetSwapchainImagesKHR(m_device, m_swapchain,
                                      &swapchain_image_count, m_swapchain_images.data());
        VK_CHECK_RESULT(err);

        // Resize swapchain buffers
        m_swapchain_buffers.resize(swapchain_image_count);
        for (auto i = 0; i < swapchain_image_count; i++)
        {
            VkImageViewCreateInfo color_image_view_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = m_surface_format.format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },

            };

            m_swapchain_buffers[i].image = m_swapchain_images[i];

            color_image_view_info.image = m_swapchain_buffers[i].image;
            err = vkCreateImageView(m_device, &color_image_view_info, nullptr, &m_swapchain_buffers[i].view);
            VK_CHECK_RESULT(err);
            assert(!err);

            // TODO: name demo objects here?
        }

        // TODO: Possibly put in code herer for VK_GOOGLE_display_timing?

        // TODO: consider moving m_swapchain_images locally here
    }

    void Renderer_Vk::create_command_buffers()
    {
        VkCommandPoolCreateInfo cmd_pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = m_graphics_queue_family_index,
        };
        VkResult err = vkCreateCommandPool(m_device, &cmd_pool_info, nullptr, &m_cmd_pool);
        assert(!err);
        VK_CHECK_RESULT(err);

        VkCommandBufferAllocateInfo cmd_alloc_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        err = vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &m_cmd_buffer);
        assert(!err);
        VK_CHECK_RESULT(err);

        // TODO: consider naming command buffer
        VkCommandBufferBeginInfo cmd_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr,
        };
        err = vkBeginCommandBuffer(m_cmd_buffer, &cmd_buffer_info);
        assert(!err);
        VK_CHECK_RESULT(err);
    }

    void Renderer_Vk::create_vertex_buffer()
    {
        VkResult err;
        // Setup vertices
        std::vector<glm::vec3> vertices_pos{
            {1.0, 1.0, 0.0},
            {-1.0, 1.0, 0.0f},
            {0.0, -1.0, 0.0}};
        std::vector<glm::vec3> vertices_color{
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}};

        VsUniform vs_data;
        glm::mat4 M{};
        glm::mat4x4 V = glm::lookAt(glm::vec3{0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, -1.0, 0.0});
        glm::mat4 P = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, .01f, 1000.0f);
        glm::mat4 MVP = P * V * M;

        VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .size = sizeof(vs_data),
        };
        VkMemoryRequirements mem_reqs;
        VkMemoryAllocateInfo alloc_info;

        for (auto i = 0; i < m_swapchain_images.size(); i++)
        {
            err = vkCreateBuffer(m_device, &buffer_info, nullptr, &m_swapchain_uniform_buffers[i]);
            assert(!err);
            VK_CHECK_RESULT(err);
            // TODO: consider naming swapchain uniform buffer
            vkGetBufferMemoryRequirements(m_device, m_swapchain_uniform_buffers[i], &mem_reqs);
            // Set allocation info
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.pNext = nullptr;
            alloc_info.allocationSize = mem_reqs.size;
            alloc_info.memoryTypeIndex = 0;

            uint32_t mem_index;
            bool found = tools::find_matching_memory(mem_reqs.memoryTypeBits,
                                                     m_phys_memory_properties.memoryTypes,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                     &mem_index);

            assert(found);
            // Allocate uniform memory
            err = vkAllocateMemory(m_device, &alloc_info, nullptr, &m_uniform_memories[i]);

            // TODO: consider naming swapchain unifrom memory object

            assert(!err);
            VK_CHECK_RESULT(err);
        }
    }

    void Renderer_Vk::create_layouts()
    {
        VkResult err;
        // Create descriptor set layout
        // Binding 0: Uniform buffer for vertex shader
        const VkDescriptorSetLayoutBinding layout_binding = {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr,
        };

        VkDescriptorSetLayoutCreateInfo layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .bindingCount = 1,
            .pBindings = &layout_binding,
        };

        err = vkCreateDescriptorSetLayout(m_device, &layout_info,
                                          nullptr, &m_descriptor_set_layout);
        VK_CHECK_RESULT(err);

        // Create pipeline layout, used to generate pipeline
        // "In a more complex scenario you would have different pipeline layouts for different descriptor set layouts that could be reused"
        VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 1,
            .pSetLayouts = &m_descriptor_set_layout,
        };
        err = vkCreatePipelineLayout(m_device, &pipeline_layout_info,
                                     nullptr, &m_pipeline_layout);
        VK_CHECK_RESULT(err);
        assert(!err);
    }

    void Renderer_Vk::prepare_depth()
    {
        const VkFormat depth_format = VK_FORMAT_D16_UNORM;
        const VkImageCreateInfo image_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depth_format,
            .extent = {1280, 720, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        };

        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = VK_NULL_HANDLE,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = depth_format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        VkMemoryRequirements mem_reqs;
        VkResult err;
        bool found;

        m_depth.format = depth_format;

        // create image
        err = vkCreateImage(m_device, &image_info, nullptr, &m_depth.image);
        assert(!err);
        VK_CHECK_RESULT(err);

        // Get depth image memory requirments
        vkGetImageMemoryRequirements(m_device, m_depth.image, &mem_reqs);
        uint32_t depth_mem_index;
        m_depth.mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        m_depth.mem_alloc.pNext = nullptr;
        m_depth.mem_alloc.allocationSize = mem_reqs.size;
        m_depth.mem_alloc.memoryTypeIndex = 0;
        found = tools::find_matching_memory(mem_reqs.memoryTypeBits,
                                            m_phys_memory_properties.memoryTypes,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depth_mem_index);
        assert(found);

        // allocate memory
        err = vkAllocateMemory(m_device, &m_depth.mem_alloc, nullptr, &m_depth.mem);

        VK_CHECK_RESULT(err);
        
        // bind memory
        err = vkBindImageMemory(m_device, m_depth.image, m_depth.mem, 0);
        // assert(!err);
        VK_CHECK_RESULT(err);
        // create image view
        view_info.image = m_depth.image;
        err = vkCreateImageView(m_device, &view_info, nullptr, &m_depth.view);

        // TODO:consider naming DepthView
    }

    void Renderer_Vk::prepare_render_pass()
    {

        // This function will prepare a single render pass with one subpass

        // Descriptors for render pass attachments
        std::array<VkAttachmentDescription, 2> attachments{};
        // Color attachment
        attachments[0].flags = 0,
        attachments[0].format = m_surface_format.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // Depth attachment

        attachments[1].flags = 0;
        attachments[1].format = m_depth.format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_ref = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkAttachmentReference depth_ref = {
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        // One subpass
        VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_ref,
            .pDepthStencilAttachment = &depth_ref,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr,
        };

        std::array<VkSubpassDependency, 2> dependencies{};
        // Depth attachment
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL,
        dependencies[0].dstSubpass = 0,
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        dependencies[0].dependencyFlags = 0;

        // Color attachment
        dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL,
        dependencies[1].dstSubpass = 0,
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        dependencies[1].srcAccessMask = 0,
        dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        dependencies[1].dependencyFlags = 0;

        VkRenderPassCreateInfo render_pass_info{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 2,
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 2,
            .pDependencies = dependencies.data(),
        };
        VkResult err;
        err = vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass);
        assert(!err);
        VK_CHECK_RESULT(err);
    }

    void Renderer_Vk::prepare_pipeline()
    {
#define NUM_DYNAMIC_STATES 2 /*Viewport and Scissor*/

        //

        VkPipelineCacheCreateInfo pipeline_cache_info{};
        VkPipelineInputAssemblyStateCreateInfo input_assem_state_info{};
        VkPipelineRasterizationStateCreateInfo raster_state_info{};
        VkPipelineColorBlendStateCreateInfo blend_state_info{};
        VkPipelineMultisampleStateCreateInfo sample_state_info{};
        VkPipelineViewportStateCreateInfo viewport_state_info{};
        VkResult err;

        input_assem_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assem_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        raster_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster_state_info.polygonMode = VK_POLYGON_MODE_FILL;
        raster_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
        raster_state_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster_state_info.depthClampEnable = VK_FALSE;
        raster_state_info.rasterizerDiscardEnable = VK_FALSE;
        raster_state_info.depthBiasEnable = VK_FALSE;
        raster_state_info.lineWidth = 1.0f;

        blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState attach_state{
            .blendEnable = VK_FALSE,
            .colorWriteMask = 0xf,
        };

        blend_state_info.attachmentCount = 1;
        blend_state_info.pAttachments = &attach_state;

        viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_info.viewportCount = 1;
        viewport_state_info.scissorCount = 1;

        std::vector<VkDynamicState> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamic_state_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pDynamicStates = dynamic_states.data(),
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .depthBoundsTestEnable = VK_FALSE,
            .back.failOp = VK_STENCIL_OP_KEEP,
            .back.passOp = VK_STENCIL_OP_KEEP,
            .back.compareOp = VK_COMPARE_OP_ALWAYS,
            .stencilTestEnable = VK_FALSE,
            .front = depth_stencil_state_info.back,
        };

        // Multi sampling state
        // Is not useed currently, but still needed
        VkPipelineMultisampleStateCreateInfo multisample_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .pSampleMask = nullptr,
        };

        // Vertex input descriptions

        // Vertex input binding
        // Single binding at point 0
        VkVertexInputBindingDescription vertex_input_binding{
            .binding = 0,
            .stride = sizeof(glm::vec3),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        // Input attribute bindings dexcribe shader attrib locations
        std::array<VkVertexInputAttributeDescription, 2> vertex_input_attribute = {
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0,
            }};

            VkPipelineVertexInputStateCreateInfo vertex_input_state_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = nullptr,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertex_input_binding,
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attribute.size()),
                .pVertexAttributeDescriptions = vertex_input_attribute.data(),
            };



        std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages = {
            // vertex shader
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = tools::createShaderModule(m_device, "../../shaders/vert.spv"),
                .pName = "main",
            },
            // fragment shader
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = tools::createShaderModule(m_device, "../../shaders/frag.spv"),
                .pName = "main",
            }};

        VkGraphicsPipelineCreateInfo pipeline_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .layout = m_pipeline_layout,
            .stageCount = static_cast<uint32_t>(shader_stages.size()),
            .pStages = shader_stages.data(),
            .pVertexInputState = &vertex_input_state_info,
            .pInputAssemblyState = &input_assem_state_info,
            .pColorBlendState = &blend_state_info,
            .pMultisampleState = &multisample_info,
            .pViewportState = &viewport_state_info,
            .pDepthStencilState = &depth_stencil_state_info,
            .pDynamicState = &dynamic_state_info,
            .renderPass = m_render_pass,
        };

        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device,
                                                  m_pipeline_cache, 1,
                                                  &pipeline_info,
                                                  nullptr,
                                                  &m_pipeline)
                                                  );

        // Shader modules are no longer needed once pipeline is created
        vkDestroyShaderModule(m_device, shader_stages[0].module, nullptr);
        vkDestroyShaderModule(m_device, shader_stages[1].module, nullptr);
    }
}
