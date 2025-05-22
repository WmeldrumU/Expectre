// Library macros
#define VMA_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define STB_IMAGE_IMPLEMENTATION // includes stb function bodies

// Renderer macros
#define RESOLUTION_X 640
#define RESOLUTION_Y 480

#define EXTENT { RESOLUTION_X, RESOLUTION_Y }

#include "rendererVk.h"

#include <iostream>
#include <set>
#include <bitset>
#include <array>
#include <chrono>
#include <cassert>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

#include "shared.h"
#include "VkTools.h"
#include "Model.h"

struct UBO
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

namespace Expectre
{

    Renderer_Vk::Renderer_Vk()
    {

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        {
            SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
            throw std::runtime_error("failed to initialize SDL!");
        }


        m_window = SDL_CreateWindow("Expectre",
            RESOLUTION_X, RESOLUTION_Y,
            SDL_WINDOW_VULKAN);

        if (!m_window)
        {
            SDL_Log("Unable to initialize application window!: %s", SDL_GetError());
            throw std::runtime_error("Unable to initialize application window!");
        }

        m_enable_validation_layers = true;


        // Core Vulkan setup
        create_instance();
        create_surface();
        select_physical_device();
        create_logical_device_and_queues();

        create_memory_allocator();

        // Command buffers and swapchain
        create_swapchain();
        create_image_views();
        create_depth();
        create_renderpass();
        create_descriptor_set_layout();
        create_pipeline();
        create_command_pool();
        create_framebuffers();
        create_texture_image();
        create_texture_image_view();
        create_texture_sampler();
        create_vertex_buffer(); // and index buffer
        create_uniform_buffers();
        create_descriptor_pool_and_sets();
        create_command_buffers();

        // Synchronization
        create_sync_objects();

        m_ready = true;
    }

    void Renderer_Vk::cleanup_swapchain() {
        // Destroy depth buffer
        vkDestroyImageView(m_device, m_depth_image_view, nullptr);
        vkDestroyImage(m_device, m_depth_image, nullptr);
        vkFreeMemory(m_device, m_depth_image_memory, nullptr);

        for (auto framebuffer : m_swapchain_framebuffers) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        for (auto imageView : m_swapchain_image_views) {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    }

    Renderer_Vk::~Renderer_Vk()
    {


        vkDeviceWaitIdle(m_device);


        // Destroy synchronization objects
        for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
            vkDestroySemaphore(m_device, m_available_image_semaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_finished_render_semaphores[i], nullptr);
            vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
        }

        vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);
        // Destroy descriptor pool
        vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

        // Destroy uniform buffers
        for (auto& ub : m_uniform_buffers) {
            vkDestroyBuffer(m_device, ub.buffer, nullptr);
            vmaUnmapMemory(m_allocator, ub.allocation);
            vmaFreeMemory(m_allocator, ub.allocation);
        }

        // Destroy vertex and index buffer
        vkDestroyBuffer(m_device, m_indices.buffer, nullptr);
        vmaFreeMemory(m_allocator, m_indices.allocation);
        vkDestroyBuffer(m_device, m_vertices.buffer, nullptr);
        vmaFreeMemory(m_allocator, m_vertices.allocation);

        // Destroy pipeline and related layouts
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);

        // Destroy texture resources
        vkDestroySampler(m_device, m_texture_sampler, nullptr);
        vkDestroyImageView(m_device, m_texture_image_view, nullptr);
        vkDestroyImage(m_device, m_texture_image, nullptr);
        vmaFreeMemory(m_allocator, m_texture_image_allocation);

        // Destroy command pool
        vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);

        cleanup_swapchain();

        vmaDestroyAllocator(m_allocator);

        // Destroy device, surface, instance
        vkDestroyDevice(m_device, nullptr);

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

        vkDestroyInstance(m_instance, nullptr);

        // SDL cleanup
        SDL_DestroyWindow(m_window);
        //SDL_Vulkan_UnloadLibrary();
        SDL_Quit();

        m_window = nullptr;
    }

    void Renderer_Vk::create_instance()
    {
        auto instance_extensions = tools::get_required_instance_extensions(m_window);

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

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};

        if (m_enable_validation_layers) {
            tools::populate_debug_messenger_create_info(debug_create_info);
            create_info.enabledLayerCount = m_validation_layers.size();
            create_info.ppEnabledLayerNames = m_validation_layers.data();
            create_info.pNext = &debug_create_info;
        }

        VK_CHECK_RESULT(vkCreateInstance(&create_info, nullptr, &m_instance));

    }

    void Renderer_Vk::create_surface()
    {
        auto err = SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface);
        // Create a Vulkan surface using SDL
        if (!err)
        {
            // Handle surface creation error
            throw std::runtime_error("Failed to create Vulkan surface");
        }
    }

    void Renderer_Vk::select_physical_device()
    {

        uint32_t gpu_count = 0;
        std::vector<VkPhysicalDevice> gpus;
        vkEnumeratePhysicalDevices(m_instance, &gpu_count, nullptr);
        gpus.resize(gpu_count);
        vkEnumeratePhysicalDevices(m_instance, &gpu_count, gpus.data());

        for (auto& gpu : gpus) {


            VkPhysicalDeviceProperties phys_properties{};
            vkGetPhysicalDeviceProperties(gpu, &phys_properties);
            std::cout << "Physical device: " << std::endl;
            std::cout << phys_properties.deviceName << std::endl;
            std::cout << "API VERSION: " << VK_API_VERSION_MAJOR(phys_properties.apiVersion) << "." << VK_API_VERSION_MINOR(phys_properties.apiVersion) << std::endl;
            std::cout << "device type: " << phys_properties.deviceType << std::endl;
            VkPhysicalDeviceFeatures phys_features{};
            vkGetPhysicalDeviceFeatures(gpu, &phys_features);

            vkGetPhysicalDeviceMemoryProperties(gpu, &m_phys_memory_properties);
            std::cout << "heap count: " << m_phys_memory_properties.memoryHeapCount << std::endl;
            for (uint32_t j = 0; j < m_phys_memory_properties.memoryHeapCount; j++)
            {
                VkMemoryHeap heap{};
                heap = m_phys_memory_properties.memoryHeaps[j];

                std::cout << "heap size:" << std::endl;
                std::cout << heap.size << std::endl;
                std::cout << "flags: " << heap.flags << std::endl;
            }


            if (phys_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                m_chosen_phys_device == VK_NULL_HANDLE)
            {
                m_chosen_phys_device = gpu;
                m_phys_memory_properties = m_phys_memory_properties;
                spdlog::debug("chose physical device: " + std::string(phys_properties.deviceName));
            }
        }

        assert(m_chosen_phys_device != VK_NULL_HANDLE);
    }

    void Renderer_Vk::create_logical_device_and_queues()
    {
        // Queue family logic
        uint32_t queue_families_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_chosen_phys_device,
            &queue_families_count,
            nullptr);
        assert(queue_families_count > 0);
        std::vector<VkQueueFamilyProperties> family_properties(queue_families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_chosen_phys_device,
            &queue_families_count,
            family_properties.data());

        // Check queues for present support
        std::vector<VkBool32> supports_present(queue_families_count);
        for (auto i = 0; i < queue_families_count; i++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(m_chosen_phys_device, i,
                m_surface, &supports_present.at(i));
        }

        // Search for queue that supports transfer, present, and graphics
        auto family_index = 0;
        for (auto i = 0; i < queue_families_count; i++)
        {
            const auto& properties = family_properties.at(i);
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
        queue_create_info.queueFamilyIndex = m_graphics_queue_family_index;

        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures(m_chosen_phys_device, &supportedFeatures);
        VkPhysicalDeviceFeatures requiredFeatures{};
        requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
        requiredFeatures.tessellationShader = VK_TRUE;
        requiredFeatures.geometryShader = VK_TRUE;
        requiredFeatures.samplerAnisotropy = VK_TRUE;

        std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pEnabledFeatures = &requiredFeatures;
        device_create_info.queueCreateInfoCount = 1;
        device_create_info.pQueueCreateInfos = &queue_create_info;
        device_create_info.enabledExtensionCount = extensions.size();
        device_create_info.ppEnabledExtensionNames = extensions.data();
        // Start creating logical device
        m_device = VK_NULL_HANDLE;
        VK_CHECK_RESULT(vkCreateDevice(m_chosen_phys_device, &device_create_info, nullptr, &m_device));

        vkGetDeviceQueue(m_device, m_graphics_queue_family_index, 0, &m_graphics_queue);
        vkGetDeviceQueue(m_device, m_present_queue_family_index, 0, &m_present_queue);
    }

    uint32_t Renderer_Vk::choose_heap_from_flags(const VkMemoryRequirements& memoryRequirements,
        VkMemoryPropertyFlags requiredFlags,
        VkMemoryPropertyFlags preferredFlags)
    {
        VkPhysicalDeviceMemoryProperties device_memory_properties;

        vkGetPhysicalDeviceMemoryProperties(m_chosen_phys_device, &device_memory_properties);

        uint32_t selected_type = ~0u; // All 1's
        uint32_t memory_type;

        for (memory_type = 0; memory_type < 32; memory_type++)
        {

            if (memoryRequirements.memoryTypeBits & (1 << memory_type))
            {
                const VkMemoryType& type = device_memory_properties.memoryTypes[memory_type];

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
                    const VkMemoryType& type = device_memory_properties.memoryTypes[memory_type];

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



    void Renderer_Vk::create_swapchain()
    {
        tools::SwapChainSupportDetails swapchain_support_details = tools::query_swap_chain_support(m_chosen_phys_device, m_surface);

        m_surface_format = tools::choose_swap_surface_format(swapchain_support_details.formats);
        //VkExtent2D extent = tools::choose_swap_extent(swapchain_support_details.capabilities, m_window);
        VkExtent2D extent = EXTENT;

        //uint32_t image_count = swapchain_support_details.capabilities.minImageCount + 1;
        uint32_t image_count = MAX_CONCURRENT_FRAMES;
        if (swapchain_support_details.capabilities.maxImageCount > 0 &&
            image_count > swapchain_support_details.capabilities.maxImageCount)
        {
            image_count = swapchain_support_details.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = m_surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = m_surface_format.format;
        create_info.imageColorSpace = m_surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


        tools::QueueFamilyIndices indices = tools::findQueueFamilies(m_chosen_phys_device, m_surface);
        uint32_t queue_family_indices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        }
        else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        create_info.preTransform = swapchain_support_details.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = PRESENT_MODE;
        create_info.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
        m_swapchain_images.resize(image_count);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());

        m_swapchain_image_format = m_surface_format.format;
        m_swapchain_extent = extent;

    }

    void Renderer_Vk::create_image_views() {
        m_swapchain_image_views.resize(m_swapchain_images.size());

        for (auto i = 0; i < m_swapchain_images.size(); i++) {
            m_swapchain_image_views[i] = tools::create_image_view(m_device, m_swapchain_images[i],
                m_swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }



    void Renderer_Vk::copy_buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = begin_single_time_commands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        end_single_time_commands(commandBuffer);
    }

    void Renderer_Vk::create_vertex_buffer()
    {



        Model model = tools::import_model("C:/src/Expectre/assets/bunny.obj");
        const std::vector<Vertex>& vertices = model.vertices;
        const std::vector<uint32_t>& indices = model.indices;
        m_indices.count = static_cast<uint32_t>(indices.size());

        VkDeviceSize vertex_buffer_size = sizeof(vertices[0]) * vertices.size();
        VkDeviceSize index_buffer_size = sizeof(indices[0]) * indices.size();

        // === STAGING BUFFERS ===
        VkBuffer vertex_staging_buffer;
        VkBuffer index_staging_buffer;
        VmaAllocation vertex_staging_alloc;
        VmaAllocation index_staging_alloc;

        VmaAllocationCreateInfo staging_alloc_info = {};
        staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        // Vertex staging buffer
        VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        buffer_info.size = vertex_buffer_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(vmaCreateBuffer(m_allocator, &buffer_info, &staging_alloc_info, &vertex_staging_buffer, &vertex_staging_alloc, nullptr));

        void* data;
        VK_CHECK_RESULT(vmaMapMemory(m_allocator, vertex_staging_alloc, &data));
        memcpy(data, vertices.data(), static_cast<size_t>(vertex_buffer_size));
        vmaUnmapMemory(m_allocator, vertex_staging_alloc);

        // Index staging buffer
        buffer_info.size = index_buffer_size;
        VK_CHECK_RESULT(vmaCreateBuffer(m_allocator, &buffer_info, &staging_alloc_info, &index_staging_buffer, &index_staging_alloc, nullptr));

        VK_CHECK_RESULT(vmaMapMemory(m_allocator, index_staging_alloc, &data));
        memcpy(data, indices.data(), static_cast<size_t>(index_buffer_size));
        vmaUnmapMemory(m_allocator, index_staging_alloc);

        // === DEVICE LOCAL BUFFERS ===
        VmaAllocationCreateInfo device_alloc_info = {};
        device_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        // Vertex buffer (device-local)
        buffer_info.size = vertex_buffer_size;
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VK_CHECK_RESULT(vmaCreateBuffer(m_allocator, &buffer_info, &device_alloc_info, &m_vertices.buffer, &m_vertices.allocation, nullptr));

        // Index buffer (device-local)
        buffer_info.size = index_buffer_size;
        buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VK_CHECK_RESULT(vmaCreateBuffer(m_allocator, &buffer_info, &device_alloc_info, &m_indices.buffer, &m_indices.allocation, nullptr));

        // === COPY ===
        copy_buffer(vertex_staging_buffer, m_vertices.buffer, vertex_buffer_size);
        copy_buffer(index_staging_buffer, m_indices.buffer, index_buffer_size);

        // Cleanup staging
        vmaDestroyBuffer(m_allocator, vertex_staging_buffer, vertex_staging_alloc);
        vmaDestroyBuffer(m_allocator, index_staging_buffer, index_staging_alloc);


    }

    void Renderer_Vk::create_depth()
    {
        m_depth_format = tools::find_depth_format(m_chosen_phys_device);

        create_image(m_swapchain_extent.width, m_swapchain_extent.height, m_depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_depth_image, m_depth_image_memory);
        m_depth_image_view = tools::create_image_view(m_device, m_depth_image, m_depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

    }

    void Renderer_Vk::create_renderpass()
    {

        // This function will prepare a single render pass with one subpass

        // Descriptors for render pass attachments
        std::array<VkAttachmentDescription, 2> attachments{};
        // Color attachment
        // attachments[0].flags = 0,
        attachments[0].format = m_swapchain_image_format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // Depth attachment

        // attachments[1].flags = 0;
        attachments[1].format = m_depth_format;
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

        std::array<VkSubpassDependency, 2> dependencies{};
        // 1) External -> Subpass 0: sync clears (color + depth)
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // 2) Subpass 0 -> External: sync store + final transition
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
        render_pass_info.pDependencies = dependencies.data();

        VkResult err;
        err = vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass);
        assert(!err);
        VK_CHECK_RESULT(err);
    }

    void Renderer_Vk::create_pipeline()
    {
        VkShaderModule vert_shader_module = tools::createShaderModule(m_device, "C:/src/Expectre/shaders/vert.vert.spv");
        VkShaderModule frag_shader_module = tools::createShaderModule(m_device, "C:/src/Expectre/shaders/frag.frag.spv");

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

        VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info,
                                   frag_shader_stage_info };

        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 4> vertex_attribute_description{};
        vertex_attribute_description[0].binding = 0;
        vertex_attribute_description[0].location = 0;
        vertex_attribute_description[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attribute_description[0].offset = offsetof(Vertex, pos);

        vertex_attribute_description[1].binding = 0;
        vertex_attribute_description[1].location = 1;
        vertex_attribute_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attribute_description[1].offset = offsetof(Vertex, color);

        vertex_attribute_description[2].binding = 0;
        vertex_attribute_description[2].location = 2;
        vertex_attribute_description[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        vertex_attribute_description[2].offset = offsetof(Vertex, normal);

        vertex_attribute_description[3].binding = 0;
        vertex_attribute_description[3].location = 3;
        vertex_attribute_description[3].format = VK_FORMAT_R32G32_SFLOAT;
        vertex_attribute_description[3].offset = offsetof(Vertex, tex_coord);

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
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
          VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info{};
        depth_stencil_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_info.depthTestEnable = VK_TRUE;
        depth_stencil_state_info.depthWriteEnable = VK_TRUE;
        depth_stencil_state_info.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil_state_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_state_info.stencilTestEnable = VK_FALSE;

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

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = m_graphics_queue_family_index;


        VK_CHECK_RESULT(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_cmd_pool));
    }

    void Renderer_Vk::create_command_buffers()
    {
        m_cmd_buffers.resize(MAX_CONCURRENT_FRAMES);

        VkCommandBufferAllocateInfo cmd_buf_info{};
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buf_info.commandPool = m_cmd_pool;
        cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buf_info.commandBufferCount = static_cast<uint32_t>(m_cmd_buffers.size());


        VK_CHECK_RESULT(vkAllocateCommandBuffers(m_device, &cmd_buf_info, m_cmd_buffers.data()));
    }

    void Renderer_Vk::create_descriptor_pool_and_sets()
    {

        // pool
        std::array<VkDescriptorPoolSize, 2> pool_sizes{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.pNext = nullptr;
        pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);

        VK_CHECK_RESULT(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool));

        // sets
        // std::vector<VkDescriptorSetLayout> layouts(MAX_CONCURRENT_FRAMES, m_descriptor_set_layout);

        for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++)
        {
            VkDescriptorSetAllocateInfo alloc_info{};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = m_descriptor_pool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &m_descriptor_set_layout;

            // m_descriptor_sets.resize(MAX_CONCURRENT_FRAMES);
            VK_CHECK_RESULT(vkAllocateDescriptorSets(m_device,
                &alloc_info,
                &m_uniform_buffers[i].descriptorSet));

            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = m_uniform_buffers[i].buffer;
            buffer_info.offset = 0;
            buffer_info.range = sizeof(UBO);

            VkDescriptorImageInfo image_info{};
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info.imageView = m_texture_image_view;
            image_info.sampler = m_texture_sampler;

            std::array<VkWriteDescriptorSet, 2> descriptor_writes{};
            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].dstSet = m_uniform_buffers[i].descriptorSet;
            descriptor_writes[0].dstBinding = 0;
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].pBufferInfo = &buffer_info;

            descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].dstSet = m_uniform_buffers[i].descriptorSet;
            descriptor_writes[1].dstBinding = 1;
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].pImageInfo = &image_info;

            vkUpdateDescriptorSets(m_device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
        }
    }

    void Renderer_Vk::create_framebuffers()
    {
        VkResult err;

        m_swapchain_framebuffers.resize(m_swapchain_image_views.size());
        for (auto i = 0; i < m_swapchain_image_views.size(); i++)
        {
            std::array<VkImageView, MAX_CONCURRENT_FRAMES> attachments{
              m_swapchain_image_views[i], m_depth_image_view };

            // All framebuffers use same renderpass setup
            VkFramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.renderPass = m_render_pass;
            framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebuffer_info.pAttachments = attachments.data();
            framebuffer_info.width = RESOLUTION_X;
            framebuffer_info.height = RESOLUTION_Y;
            framebuffer_info.layers = 1;

            // Create framebuffer
            err = vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_swapchain_framebuffers[i]);
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

    void Renderer_Vk::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index)
    {
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.pNext = nullptr;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        begin_info.pInheritanceInfo = nullptr;

        VK_CHECK_RESULT(vkBeginCommandBuffer(command_buffer, &begin_info));

        std::array<VkClearValue, 2> clear_col;
        clear_col[0] = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        clear_col[1].depthStencil = { 1.0f, 0 };
        VkRenderPassBeginInfo renderpass_info = {};
        renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderpass_info.pNext = nullptr;
        renderpass_info.renderPass = m_render_pass;
        renderpass_info.framebuffer = m_swapchain_framebuffers[image_index];
        renderpass_info.renderArea.offset.x = 0;
        renderpass_info.renderArea.offset.y = 0;
        renderpass_info.renderArea.extent.width = RESOLUTION_X;
        renderpass_info.renderArea.extent.height = RESOLUTION_Y;
        renderpass_info.clearValueCount = 2;
        renderpass_info.pClearValues = clear_col.data();

        vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)RESOLUTION_X;
        viewport.height = (float)RESOLUTION_Y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = RESOLUTION_X;
        scissor.extent.height = RESOLUTION_Y;

        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_vertices.buffer, offsets);

        vkCmdBindIndexBuffer(command_buffer, m_indices.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline_layout, 0, 1,
            &m_uniform_buffers[m_current_frame].descriptorSet,
            0, nullptr);

        vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(m_indices.count), 1, 0, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));
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
        update_uniform_buffer();
        vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);
        vkResetCommandBuffer(m_cmd_buffers[m_current_frame], 0);
        record_command_buffer(m_cmd_buffers[m_current_frame], image_index);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] =
        { m_available_image_semaphores[m_current_frame] };
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_cmd_buffers[m_current_frame];

        VkSemaphore signal_semaphores[] =
        { m_finished_render_semaphores[m_current_frame] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        VK_CHECK_RESULT(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences[m_current_frame]));

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swapchains[] = { m_swapchain };
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;

        present_info.pImageIndices = &image_index;
        result = vkQueuePresentKHR(m_present_queue, &present_info);
        VK_CHECK_RESULT(result);
        m_current_frame = (m_current_frame + 1) % MAX_CONCURRENT_FRAMES;
    }

    void Renderer_Vk::create_descriptor_set_layout()
    {
        VkDescriptorSetLayoutBinding ubo_layout_binding{};
        ubo_layout_binding.binding = 0;
        ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_layout_binding.descriptorCount = 1;
        ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        ubo_layout_binding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding sampler_layout_binding{};
        sampler_layout_binding.binding = 1;
        sampler_layout_binding.descriptorCount = 1;
        sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_layout_binding.pImmutableSamplers = nullptr;
        sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, sampler_layout_binding };
        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_descriptor_set_layout));

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.setLayoutCount = 1;
        // pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;

        VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout));
    }

    void Renderer_Vk::create_uniform_buffers()
    {

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = sizeof(UBO);
        buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Host-visible and coherent by default

        for (int i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
        {
            // Create buffer + allocate memory
            VK_CHECK_RESULT(vmaCreateBuffer(
                m_allocator,
                &buffer_info,
                &alloc_info,
                &m_uniform_buffers[i].buffer,
                &m_uniform_buffers[i].allocation,
                nullptr));

            // Map once for persistent updates
            VK_CHECK_RESULT(vmaMapMemory(
                m_allocator,
                m_uniform_buffers[i].allocation,
                reinterpret_cast<void**>(&m_uniform_buffers[i].mapped)));
        }
    }

    void Renderer_Vk::update_uniform_buffer()
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        glm::vec3 start{ 0.0f, 3.0f, -2.0f };
        glm::vec3 end{ 0.0f, 3.0f, -5.0f };
        float t = sin(time / 4.0f * glm::pi<float>()) * 0.5f + 0.5f;

        //glm::vec3 camera_pos = (1.0f - t) * start + t * end;

        UBO ubo{};
        //ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.model = glm::mat4(1.0f);
        ubo.view = glm::lookAt(m_camera.pos, m_camera.forward_dir, glm::vec3(0.0f, 1.0f, 0.0f));
        ubo.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(RESOLUTION_X) / RESOLUTION_Y, 0.1f, 1000.0f);

        ubo.projection[1][1] *= -1;

        memcpy(m_uniform_buffers[m_current_frame].mapped, &ubo, sizeof(ubo));
    }

    void Renderer_Vk::create_texture_image()
    {
        int tex_width, tex_height, tex_channels;
        stbi_uc* pixels = stbi_load("C:/src/Expectre/assets/textures/hello4.jpg",
            &tex_width, &tex_height,
            &tex_channels, STBI_rgb_alpha);

        if (!pixels)
        {
            spdlog::error("Failed to load texture!");
            std::terminate();
        }

        VkDeviceSize image_size = tex_width * tex_height * 4;

        // 1. Create staging buffer (host-visible)
        VkBuffer staging_buffer;
        VmaAllocation staging_alloc;

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = image_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo buffer_alloc_info = {};
        buffer_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VK_CHECK_RESULT(vmaCreateBuffer(m_allocator,
            &buffer_info,
            &buffer_alloc_info,
            &staging_buffer,
            &staging_alloc,
            nullptr));

        // 2. Upload image data to the staging buffer
        void* data;
        VK_CHECK_RESULT(vmaMapMemory(m_allocator, staging_alloc, &data));
        memcpy(data, pixels, static_cast<size_t>(image_size));
        vmaUnmapMemory(m_allocator, staging_alloc);

        stbi_image_free(pixels);

        // 3. Create GPU texture image (device-local)
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = static_cast<uint32_t>(tex_width);
        image_info.extent.height = static_cast<uint32_t>(tex_height);
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.flags = 0;

        VmaAllocationCreateInfo image_alloc_info = {};
        image_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK_RESULT(vmaCreateImage(m_allocator,
            &image_info,
            &image_alloc_info,
            &m_texture_image,
            &m_texture_image_allocation,
            nullptr));

        // 4. Transfer layout and copy data
        transition_image_layout(m_texture_image, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        copy_buffer_to_image(staging_buffer, m_texture_image,
            static_cast<uint32_t>(tex_width),
            static_cast<uint32_t>(tex_height));

        transition_image_layout(m_texture_image, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // 5. Cleanup staging buffer
        vmaDestroyBuffer(m_allocator, staging_buffer, staging_alloc);
    }


    bool Renderer_Vk::isReady() { return m_ready; };

    void Renderer_Vk::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkBuffer& buffer,
        VkDeviceMemory& buffer_memory)
    {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(vkCreateBuffer(m_device, &buffer_info, nullptr, &buffer));

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(m_device, buffer, &mem_reqs);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        bool found = tools::find_matching_memory(mem_reqs.memoryTypeBits,
            m_phys_memory_properties.memoryTypes,
            properties, &alloc_info.memoryTypeIndex);
        assert(found);

        VK_CHECK_RESULT(vkAllocateMemory(m_device, &alloc_info, nullptr, &buffer_memory));

        vkBindBufferMemory(m_device, buffer, buffer_memory, 0);
    }

    void Renderer_Vk::create_image(uint32_t width, uint32_t height,
        VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& image_memory)
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = tiling;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK_RESULT(vkCreateImage(m_device, &image_info, nullptr, &image));

        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(m_device, image, &mem_reqs);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        bool found = tools::find_matching_memory(mem_reqs.memoryTypeBits, m_phys_memory_properties.memoryTypes,
            properties, &alloc_info.memoryTypeIndex);
        assert(found);
        VK_CHECK_RESULT(vkAllocateMemory(m_device, &alloc_info, nullptr, &image_memory));

        vkBindImageMemory(m_device, image, image_memory, 0);
    }

    void Renderer_Vk::copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        VkCommandBuffer cmd_buffer = begin_single_time_commands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
          width,
          height,
          1 };

        vkCmdCopyBufferToImage(cmd_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        end_single_time_commands(cmd_buffer);
    }

    VkCommandBuffer Renderer_Vk::begin_single_time_commands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_cmd_pool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void Renderer_Vk::end_single_time_commands(VkCommandBuffer cmd_buffer)
    {
        vkEndCommandBuffer(cmd_buffer);

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_buffer;

        vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphics_queue);

        vkFreeCommandBuffers(m_device, m_cmd_pool, 1, &cmd_buffer);
    }

    void Renderer_Vk::transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
    {
        VkCommandBuffer cmd_buffer = begin_single_time_commands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = old_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            cmd_buffer,
            source_stage, dest_stage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        end_single_time_commands(cmd_buffer);
    }

    void Renderer_Vk::create_texture_image_view()
    {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_texture_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VK_CHECK_RESULT(vkCreateImageView(m_device,
            &view_info,
            nullptr, &m_texture_image_view));
    }
    void Renderer_Vk::create_texture_sampler()
    {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.anisotropyEnable = VK_TRUE;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_chosen_phys_device, &properties);
        sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE; // Sample with [0, 1] range
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;

        VK_CHECK_RESULT(vkCreateSampler(m_device, &sampler_info, nullptr, &m_texture_sampler));
    }

    void Renderer_Vk::update(uint64_t delta_t)
    {
        // update camera velocity
        glm::vec3 dir(0.0f);

        if (m_camera.moveForward)
            dir += glm::vec3(0.0f, 0.0f, -1.0f);
        if (m_camera.moveBack)
            dir += glm::vec3(0.0f, 0.0f, 1.0f);
        if (m_camera.moveLeft)
            dir += glm::vec3(-1.0f, 0.0f, 0.0f);
        if (m_camera.moveRight)
            dir += glm::vec3(1.0f, 0.0f, 0.0f);

        if (glm::length(dir) > 0.0f)
            dir = glm::normalize(dir);

        m_camera.pos += dir * m_camera.camera_speed * static_cast<float>(delta_t) / 1000.0f;

    }
    void Renderer_Vk::on_input_event(const SDL_Event& event)
    {

        // Check for up keys
        const bool is_up = (event.type == SDL_EVENT_KEY_UP);

        if (event.type == SDL_EVENT_KEY_UP)
        {
            switch (event.key.key)
            {
            case SDLK_W:
                m_camera.moveForward = is_up;
                break;
            case SDLK_S:
                m_camera.moveBack = is_up;
                break;
            case SDLK_A:
                m_camera.moveLeft = is_up;
                break;
            case SDLK_D:
                m_camera.moveRight = is_up;
                break;
            }
        }

        // Check for down keys
        const bool is_down = (event.type == SDL_EVENT_KEY_DOWN);
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            switch (event.key.key)
            {
            case SDLK_W:
                m_camera.moveForward = is_down;
                break;
            case SDLK_S:
                m_camera.moveBack = is_down;
                break;
            case SDLK_A:
                m_camera.moveLeft = is_down;
                break;
            case SDLK_D:
                m_camera.moveRight = is_down;
                break;
            }
        }
    }


    void Renderer_Vk::create_memory_allocator() {
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        allocatorCreateInfo.physicalDevice = m_chosen_phys_device;
        allocatorCreateInfo.device = m_device;
        allocatorCreateInfo.instance = m_instance;

        vmaCreateAllocator(&allocatorCreateInfo, &m_allocator);
    }

}