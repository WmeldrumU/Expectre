
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

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
};

// struct
// {
//     glm::mat4 modelMatrix;
//     glm::mat4 projectionMatrix;
//     glm::mat4 viewMatrix;
// } uboVS;

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
        m_window = SDL_CreateWindow("Expectre",
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

        create_logical_device_and_queues();

        query_swap_chain_support();

        get_surface_format();

        create_command_pool();

        create_swapchain();

        create_command_buffers();

        prepare_depth();

        create_renderpass();

        create_framebuffers();

        create_vertex_buffer();

        create_pipeline();

        create_sync_objects();

        m_ready = true;
    }

    Renderer_Vk::~Renderer_Vk()
    {
        vkDeviceWaitIdle(m_device);

        // Destroy
        for (auto i = 0; i < m_framebuffers.size(); i++)
        {
            vkDestroyFramebuffer(m_device, m_framebuffers[i], nullptr);
        }

        // Destroy uniform buffers & image views
        for (auto i = 0; i < m_swapchain_buffers.size(); i++)
        {
            vkDestroyImageView(m_device, m_swapchain_buffers[i].view, nullptr);
        }

        for (auto i = 0; i < m_uniform_buffers.size(); i++)
        {
            vkDestroyBuffer(m_device, m_uniform_buffers[i].buffer, nullptr);
            vkFreeMemory(m_device, m_uniform_buffers[i].memory, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);

        vkDestroyBuffer(m_device, m_indices.buffer, nullptr);
        vkFreeMemory(m_device, m_indices.memory, nullptr);

        vkDestroyBuffer(m_device, m_vertices.buffer, nullptr);
        vkFreeMemory(m_device, m_vertices.memory, nullptr);

        // might not need this
        // vkDestroyBuffer(m_device, m_buffer, nullptr);

        for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++)
        {
            vkDestroySemaphore(m_device, m_available_image_semaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_finished_render_semaphores[i], nullptr);
            vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
        }
        vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);
        vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
        vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
        // vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

        // Destroy depth buffer
        vkDestroyImageView(m_device, m_depth.view, nullptr);
        vkDestroyImage(m_device, m_depth.image, nullptr);
        vkFreeMemory(m_device, m_depth.mem, nullptr);

        // vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        // vkDestroyImageView(m_device, m_image_view, nullptr);
        vkDestroyImage(m_device, m_image, nullptr);
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
        for (const char *ext : extensions)
        {
            std::printf("%s", ext);
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
        err = vkEnumeratePhysicalDevices(m_instance, &physical_device_count, m_physical_devices.data());
        assert(!err);
        // Report available devices
        for (uint32_t i = 0; i < m_physical_devices.size(); i++)
        {
            VkPhysicalDevice &phys_device = m_physical_devices.at(i);
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
        vkGetDeviceQueue(m_device, family_index, 0, &m_present_queue);
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

    void Renderer_Vk::create_swapchain()
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
            .minImageCount = std::min(capabilities.maxImageCount,
                                      static_cast<uint32_t>(MAX_CONCURRENT_FRAMES)),
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

    void Renderer_Vk::create_vertex_buffer()
    {
        VkResult err;
        // Setup vertices
        // std::vector<glm::vec3> vertices_pos{
        //     {1.0, 1.0, 0.0},
        //     {-1.0, 1.0, 0.0f},
        //     {0.0, -1.0, 0.0}};
        std::vector<Vertex> vertices{
            {{-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};
        uint32_t vertex_buffer_size =
            static_cast<uint32_t>((vertices.size() * sizeof(Vertex)));
        // Setup indices
        std::vector<uint32_t> index_buffer{0, 1, 2};
        m_indices.count = static_cast<uint32_t>(index_buffer.size());
        uint32_t index_buffer_size = m_indices.count * sizeof(index_buffer[0]);

        VkMemoryAllocateInfo mem_alloc{};
        mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements mem_reqs;

        struct StagingBuffer
        {
            VkDeviceMemory memory;
            VkBuffer buffer;
        };

        struct
        {
            StagingBuffer vertices;
            StagingBuffer indices;
        } staging_buffers;

        void *data;

        // Vertex Buffer
        VkBufferCreateInfo vertex_buffer_info{};
        vertex_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertex_buffer_info.size = vertex_buffer_size;
        vertex_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        // Create host-visible buffer to copy vertices into
        err = vkCreateBuffer(m_device, &vertex_buffer_info, nullptr,
                             &staging_buffers.vertices.buffer);
        VK_CHECK_RESULT(err);
        // Get the memory requirements for the buffer
        uint32_t mem_index;
        mem_alloc.allocationSize = mem_reqs.size;
        // Get memory index and assign to mem_alloc info
        vkGetBufferMemoryRequirements(m_device, staging_buffers.vertices.buffer, &mem_reqs);
        bool found = tools::find_matching_memory(mem_reqs.memoryTypeBits,
                                                 m_phys_memory_properties.memoryTypes,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                 &mem_index);
        assert(found);
        mem_alloc.allocationSize = mem_reqs.size;
        mem_alloc.memoryTypeIndex = mem_index;

        err = vkAllocateMemory(m_device, &mem_alloc, nullptr, &staging_buffers.vertices.memory);
        VK_CHECK_RESULT(err);
        // Map the memory, then copy it
        err = vkMapMemory(m_device, staging_buffers.vertices.memory, 0, mem_alloc.allocationSize, 0, &data);
        VK_CHECK_RESULT(err);
        memcpy(data, vertices.data(), vertex_buffer_size);
        vkUnmapMemory(m_device, staging_buffers.vertices.memory);
        err = vkBindBufferMemory(m_device, staging_buffers.vertices.buffer, staging_buffers.vertices.memory, 0);
        VK_CHECK_RESULT(err);

        // Create device local buffer that will receive vertex data from host
        vertex_buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        err = vkCreateBuffer(m_device, &vertex_buffer_info, nullptr, &m_vertices.buffer);
        VK_CHECK_RESULT(err);
        vkGetBufferMemoryRequirements(m_device, m_vertices.buffer, &mem_reqs);
        mem_alloc.allocationSize = mem_reqs.size;
        found = tools::find_matching_memory(mem_reqs.memoryTypeBits, m_phys_memory_properties.memoryTypes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_index);
        assert(found);
        mem_alloc.memoryTypeIndex = found;
        err = vkAllocateMemory(m_device, &mem_alloc, nullptr, &m_vertices.memory);
        VK_CHECK_RESULT(err);
        err = vkBindBufferMemory(m_device, m_vertices.buffer, m_vertices.memory, 0);
        VK_CHECK_RESULT(err);

        // Index buffer
        // uint32_t index_buffer_size = sizeof(index_buffer[0]) * index_buffer.size();

        VkBufferCreateInfo index_buffer_info{};
        index_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        index_buffer_info.size = index_buffer_size;
        index_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        // Copy index data to host-visible buffer aka staging buffer
        err = vkCreateBuffer(m_device, &index_buffer_info, nullptr, &staging_buffers.indices.buffer);
        VK_CHECK_RESULT(err);

        vkGetBufferMemoryRequirements(m_device, staging_buffers.indices.buffer, &mem_reqs);
        mem_alloc.allocationSize = mem_reqs.size;
        found = tools::find_matching_memory(mem_reqs.memoryTypeBits,
                                            m_phys_memory_properties.memoryTypes,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            &mem_index);

        assert(found);
        mem_alloc.memoryTypeIndex = mem_index;
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &mem_alloc, nullptr, &staging_buffers.indices.memory));
        VK_CHECK_RESULT(vkMapMemory(m_device, staging_buffers.indices.memory, 0, index_buffer_size, 0, &data));
        memcpy(data, index_buffer.data(), index_buffer_size);
        vkUnmapMemory(m_device, staging_buffers.indices.memory);
        err = vkBindBufferMemory(m_device, staging_buffers.indices.buffer, staging_buffers.indices.memory, 0);
        VK_CHECK_RESULT(err);

        // Create destination buffer that only the GPU can see
        index_buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VK_CHECK_RESULT(vkCreateBuffer(m_device, &index_buffer_info, nullptr, &m_indices.buffer));
        vkGetBufferMemoryRequirements(m_device, m_indices.buffer, &mem_reqs);
        mem_alloc.allocationSize = mem_reqs.size;
        found = tools::find_matching_memory(mem_reqs.memoryTypeBits,
                                            m_phys_memory_properties.memoryTypes,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            &mem_index);
        assert(found);
        mem_alloc.memoryTypeIndex = mem_index;
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &mem_alloc, nullptr, &m_indices.memory));
        VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_indices.buffer, m_indices.memory, 0));

        // Buffer copies have to be submitted to a queue, so we need a command buffer for them
        // Note: Some devices offer a dedicated transfer queue (with only the transfer bit set) that may be faster when doing lots of copies
        VkCommandBuffer copy_cmd;

        VkCommandBufferAllocateInfo command_buffer_alloc_info{};
        command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_alloc_info.commandPool = m_cmd_pool;
        command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_alloc_info.commandBufferCount = static_cast<uint32_t>(m_cmd_buffers.size());
        VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &command_buffer_alloc_info, &copy_cmd));

        VkCommandBufferBeginInfo cmd_buffer_info{};
        cmd_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK_RESULT(vkBeginCommandBuffer(copy_cmd, &cmd_buffer_info));
        // Put buffer region copies into command buffer
        VkBufferCopy copy_region{};
        // Vertex buffer
        copy_region.size = vertex_buffer_size;
        vkCmdCopyBuffer(copy_cmd, staging_buffers.vertices.buffer, m_vertices.buffer, 1, &copy_region);
        // Index buffer
        copy_region.size = index_buffer_size;
        vkCmdCopyBuffer(copy_cmd, staging_buffers.indices.buffer, m_indices.buffer, 1, &copy_region);
        VK_CHECK_RESULT(vkEndCommandBuffer(copy_cmd));

        // Submit the command buffer to the queue to finish the copy
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &copy_cmd;

        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = 0;
        VkFence fence;
        VK_CHECK_RESULT(vkCreateFence(m_device, &fence_info, nullptr, &fence));

        // Submit to the queue
        VK_CHECK_RESULT(vkQueueSubmit(m_graphics_queue, 1, &submit_info, fence));
        // Wait for the fence to signal that command buffer has finished executing
        VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

        vkDestroyFence(m_device, fence, nullptr);
        vkFreeCommandBuffers(m_device, m_cmd_pool, 1, &copy_cmd);

        // Destroy staging buffers
        // Note: Staging buffer must not be deleted before the copies have been submitted and executed
        vkDestroyBuffer(m_device, staging_buffers.vertices.buffer, nullptr);
        vkFreeMemory(m_device, staging_buffers.vertices.memory, nullptr);
        vkDestroyBuffer(m_device, staging_buffers.indices.buffer, nullptr);
        vkFreeMemory(m_device, staging_buffers.indices.memory, nullptr);
    }

    void Renderer_Vk::prepare_depth()
    {
        const VkFormat depth_format = VK_FORMAT_D16_UNORM;
        VkImageCreateInfo image_info = {
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

    void Renderer_Vk::create_renderpass()
    {

        // This function will prepare a single render pass with one subpass

        // Descriptors for render pass attachments
        std::array<VkAttachmentDescription, 2> attachments{};
        // Color attachment
        // attachments[0].flags = 0,
        attachments[0].format = m_surface_format.format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // Depth attachment

        // attachments[1].flags = 0;
        attachments[1].format = m_depth.format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                           // Clear depth at start of first subpass
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;                     // We don't need depth after render pass has finished (DONT_CARE may result in better performance)
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                // No stencil
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;              // No Stencil
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                      // Layout at render pass start. Initial doesn't matter, so we use undefined
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Transition to depth/stencil attachment

        VkAttachmentReference color_ref = {};
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_ref = {};
        depth_ref.attachment = 1;
        depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // One subpass
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;
        subpass.pDepthStencilAttachment = &depth_ref;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;
        subpass.pResolveAttachments = nullptr;

        std::array<VkSubpassDependency, 2> dependencies{};
        // Color attachment
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dependencies[0].dependencyFlags = 0;
        // Depth attachment
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dependencyFlags = 0;

        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
        render_pass_info.pDependencies = dependencies.data();
        // .dependencyCount = 2,
        // .pDependencies = dependencies.data(),
        VkResult err;
        err = vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass);
        assert(!err);
        VK_CHECK_RESULT(err);
    }

    void Renderer_Vk::create_pipeline()
    {
        VkShaderModule vert_shader_module = tools::createShaderModule(m_device, "../../shaders/vert.vert.spv");
        VkShaderModule frag_shader_module = tools::createShaderModule(m_device, "../../shaders/frag.frag.spv");

        VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
        vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_shader_stage_info.module = vert_shader_module;
        vert_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
        frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_shader_stage_info.module = frag_shader_module;
        frag_shader_stage_info.pName = "main";

        VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info,
                                                           frag_shader_stage_info};

        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 2> vertex_attribute_description{};
        vertex_attribute_description[0].binding = 0;
        vertex_attribute_description[0].location = 0;
        vertex_attribute_description[0].format = VK_FORMAT_R32G32_SFLOAT;
        vertex_attribute_description[0].offset = offsetof(Vertex, pos);

        vertex_attribute_description[1].binding = 0;
        vertex_attribute_description[1].location = 1;
        vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attribute_description[1].offset = offsetof(Vertex, color);

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(vertex_attribute_description.size());
        vertex_input_info.pVertexBindingDescriptions = &binding_description;
        vertex_input_info.pVertexAttributeDescriptions = vertex_attribute_description.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multi_sample_info{};
        multi_sample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multi_sample_info.sampleShadingEnable = VK_FALSE;
        multi_sample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState color_blend_attach{};
        color_blend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attach.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blend_info{};
        color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_info.logicOpEnable = VK_FALSE;
        color_blend_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_info.attachmentCount = 1;
        color_blend_info.pAttachments = &color_blend_attach;
        color_blend_info.blendConstants[0] = 0.0f;
        color_blend_info.blendConstants[1] = 0.0f;
        color_blend_info.blendConstants[2] = 0.0f;
        color_blend_info.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info{};
        depth_stencil_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_info.depthTestEnable = VK_TRUE;
        depth_stencil_state_info.depthWriteEnable = VK_TRUE;
        depth_stencil_state_info.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_stencil_state_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_state_info.back.failOp = VK_STENCIL_OP_KEEP;
        depth_stencil_state_info.back.passOp = VK_STENCIL_OP_KEEP;
        depth_stencil_state_info.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil_state_info.stencilTestEnable = VK_FALSE;
        depth_stencil_state_info.front = depth_stencil_state_info.back;

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pushConstantRangeCount = 0;

        VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout));

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pDepthStencilState = &depth_stencil_state_info;
        pipeline_info.pMultisampleState = &multi_sample_info;
        pipeline_info.pColorBlendState = &color_blend_info;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = m_pipeline_layout;
        pipeline_info.renderPass = m_render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

        VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline));

        vkDestroyShaderModule(m_device, frag_shader_module, nullptr);
        vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
    }

    void Renderer_Vk::create_command_pool()
    {

        VkCommandPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_graphics_queue_family_index,
        };

        VK_CHECK_RESULT(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_cmd_pool));
    }

    void Renderer_Vk::create_command_buffers()
    {
        m_cmd_buffers.resize(MAX_CONCURRENT_FRAMES);

        VkCommandBufferAllocateInfo cmd_buf_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(m_cmd_buffers.size()),
        };

        VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmd_buf_info, m_cmd_buffers.data()));
    }

    void Renderer_Vk::create_descriptor_pool_and_sets()
    {
        std::array<VkDescriptorPoolSize, 1> pool_sizes{};

        pool_sizes[0].descriptorCount = static_cast<uint32_t>(m_swapchain_images.size());
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(m_swapchain_images.size());

        VkDescriptorPoolCreateInfo descriptor_pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .maxSets = static_cast<uint32_t>(m_swapchain_images.size()),
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data(),
        };
        VkResult err;
        err = vkCreateDescriptorPool(m_device, &descriptor_pool_info, nullptr, &m_descriptor_pool);
        VK_CHECK_RESULT(err);
        assert(!err);

        m_descriptor_sets.resize(m_swapchain_images.size());

        // Prepare descriptor set
        for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; i++)
        {
            VkDescriptorSetAllocateInfo alloc_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = m_descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &m_descriptor_set_layout,
            };
            // Use one UBO per frame
            err = vkAllocateDescriptorSets(m_device, &alloc_info, &m_descriptor_sets[i]);
            VK_CHECK_RESULT(err);
            assert(!err);
            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = m_uniform_buffers[i].buffer;
            buffer_info.range = sizeof(glm::vec3);

            // Binding 0: Uniform buffer
            VkWriteDescriptorSet write_descriptor_set{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptor_sets[i],
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info,
            };
            vkUpdateDescriptorSets(m_device, 1, &write_descriptor_set, 0, nullptr);
        }
    }

    void Renderer_Vk::create_framebuffers()
    {
        VkResult err;
        m_framebuffers.resize(m_swapchain_images.size());
        for (auto i = 0; i < m_framebuffers.size(); i++)
        {
            std::array<VkImageView, 2> attachments;

            attachments[0] = m_swapchain_buffers[i].view;
            attachments[1] = m_depth.view;

            // All framebuffers use same renderpass setup
            VkFramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = m_render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = 1280;
            framebuffer_info.height = 720;
            framebuffer_info.layers = 1;

            // Create framebuffer
            err = vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_framebuffers[i]);
            VK_CHECK_RESULT(err);
        }
    }

    void Renderer_Vk::create_sync_objects()
    {
        m_available_image_semaphores.resize(MAX_CONCURRENT_FRAMES);
        m_finished_render_semaphores.resize(MAX_CONCURRENT_FRAMES);
        m_in_flight_fences.resize(MAX_CONCURRENT_FRAMES);

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++)
        {
            // For each swapchain image
            // Create an "available" and "finished" semphore
            // Create fence as well
            auto avail =
                vkCreateSemaphore(m_device,
                                  &semaphore_info,
                                  nullptr,
                                  &m_available_image_semaphores[i]);
            auto finished =
                vkCreateSemaphore(m_device,
                                  &semaphore_info,
                                  nullptr,
                                  &m_finished_render_semaphores[i]);
            auto fences =
                vkCreateFence(m_device,
                              &fence_info,
                              nullptr,
                              &m_in_flight_fences[i]);

            VK_CHECK_RESULT(avail);
            VK_CHECK_RESULT(finished);
            VK_CHECK_RESULT(fences);
        }
    }

    void Renderer_Vk::record_command_buffer()
    {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = nullptr;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        begin_info.pInheritanceInfo = nullptr;

        VkCommandBuffer current_cmd_buffer = m_cmd_buffers[m_current_frame];
        VK_CHECK_RESULT(vkBeginCommandBuffer(current_cmd_buffer, &begin_info));

        std::array<VkClearValue, 2> clear_col;
        clear_col[0] = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        clear_col[1] = {{{0.5f, 0.2f, 0.2f, 1.0f}}};
        VkRenderPassBeginInfo renderpass_info = {};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.pNext = nullptr;
        renderpass_info.renderPass = m_render_pass;
        renderpass_info.framebuffer = m_framebuffers[m_current_frame];
        renderpass_info.renderArea.offset.x = 0;
        renderpass_info.renderArea.offset.y = 0;
        renderpass_info.renderArea.extent.width = 1280;
        renderpass_info.renderArea.extent.height = 720;
        renderpass_info.clearValueCount = 2;
        renderpass_info.pClearValues = clear_col.data();

        vkCmdBeginRenderPass(current_cmd_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(current_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = 1280.0f,
            .height = 720.0f,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(current_cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = 1280;
        scissor.extent.height = 720;

        vkCmdSetScissor(current_cmd_buffer, 0, 1, &scissor);

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(current_cmd_buffer, 0, 1, &m_vertices.buffer, offsets);

        vkCmdBindIndexBuffer(current_cmd_buffer, m_indices.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(current_cmd_buffer, static_cast<uint32_t>(m_indices.count), 1, 0, 0, 0);

        vkCmdEndRenderPass(current_cmd_buffer);

        VK_CHECK_RESULT(vkEndCommandBuffer(current_cmd_buffer));
    }

    void Renderer_Vk::draw_frame()
    {
        vkWaitForFences(m_device, 1, &m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult result =
            vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                  m_available_image_semaphores[m_current_frame],
                                  VK_NULL_HANDLE, &image_index);
        VK_CHECK_RESULT(result);

        vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);

        vkResetCommandBuffer(m_cmd_buffers[m_current_frame], 0);
        record_command_buffer();

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] =
            {m_available_image_semaphores[m_current_frame]};
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_cmd_buffers[m_current_frame];

        VkSemaphore signal_semaphores[] =
            {m_finished_render_semaphores[m_current_frame]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        VK_CHECK_RESULT(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences[m_current_frame]));

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swapchains[] = {m_swapchain};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;

        present_info.pImageIndices = &image_index;
        result = vkQueuePresentKHR(m_present_queue, &present_info);
        VK_CHECK_RESULT(result);
        m_current_frame = (m_current_frame + 1) % MAX_CONCURRENT_FRAMES;
    }

    bool Renderer_Vk::isReady() { return m_ready; };
}