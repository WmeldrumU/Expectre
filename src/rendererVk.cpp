// Library macros
#define VMA_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define STB_IMAGE_IMPLEMENTATION // includes stb function bodies

#include "RendererVk.h"

#include <iostream>
#include <set>
#include <bitset>
#include <array>
#include <chrono>
#include <cassert>


#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

#include "shared.h"
#include "ToolsVk.h"
#include "Model.h"
#include "ShaderFileWatcher.h"
#include "UIRendererVkNoesis.h"
#include "Time.h"
#include <NsRender/Texture.h>
#include <NsRender/RenderTarget.h>
#include <NsCore/Log.h>
#include <NsGui/IntegrationAPI.h>
#include <NsGui/IView.h>
#include <NsGui/IRenderer.h>

struct MVP_uniform_object
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

namespace Expectre
{

	RendererVk::RendererVk(SDL_Window* window, uint32_t resolution_x, uint32_t resolution_y) : m_window(window), m_extent{ resolution_x, resolution_y }
	{

		m_enable_validation_layers = true;

		// Core Vulkan setup
		create_instance();
		create_surface();
		select_physical_device();
		create_logical_device_and_queues();
		create_memory_allocator();

		// Command buffers and swapchain
		create_swapchain();

		m_swapchain_image_views.resize(m_swapchain_images.size());
		for (auto i = 0; i < m_swapchain_images.size(); i++) {
			m_swapchain_image_views[i] = create_swapchain_image_views(m_device, m_swapchain_images[i],
				m_swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		m_cmd_pool = create_command_pool(m_device, m_graphics_queue_family_index);
		m_depth_stencil = TextureVk::create_depth_stencil(m_chosen_phys_device, m_device, m_cmd_pool, m_graphics_queue, m_allocator, m_extent);
		//m_render_pass = ToolsVk::create_renderpass(m_device, VK_SAMPLE_COUNT_1_BIT, m_swapchain_image_format, m_depth_stencil.image_info.format, "main_renderpass");
		m_render_pass = create_renderpass(m_device, m_swapchain_image_format, m_depth_stencil.image_info.format, true);

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

		m_descriptor_set_layout = create_descriptor_set_layout({ ubo_layout_binding, sampler_layout_binding });
		m_pipeline_layout = create_pipeline_layout(m_device, m_descriptor_set_layout);
		m_pipeline = create_pipeline(m_device, m_render_pass, m_pipeline_layout);
		m_swapchain_framebuffers.resize(m_swapchain_image_views.size());
		for (auto i = 0; i < m_swapchain_image_views.size(); i++) {
			VkImageView view = m_swapchain_image_views[i];
			m_swapchain_framebuffers[i] = create_framebuffer(m_device, m_swapchain_image_views[i], m_depth_stencil.view);
		}
		// std::ignore = create_texture_from_file(WORKSPACE_DIR + std::string("/assets/teapot/brick.png"));
		m_texture = TextureVk::create_texture_from_file(m_device, m_cmd_pool, m_graphics_queue, m_allocator, WORKSPACE_DIR + std::string("/assets/teapot/brick.png"));
		m_texture_sampler = ToolsVk::create_texture_sampler(m_chosen_phys_device, m_device);
		m_models.push_back(load_model(WORKSPACE_DIR + std::string("/assets/teapot/teapot.obj")));
		m_models.push_back(load_model(WORKSPACE_DIR + std::string("/assets/bunny.obj")));
		create_geometry_buffer();

		std::vector<VkDescriptorPoolSize> pool_sizes(2);
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_sizes[0].descriptorCount = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[1].descriptorCount = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);
		m_descriptor_pool = create_descriptor_pool(m_device, pool_sizes);

		for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++) {
			auto& uniform_buffer = m_uniform_buffers[i];
			uniform_buffer = create_uniform_buffer(m_allocator, sizeof(MVP_uniform_object));
			uniform_buffer.descriptorSet =
				create_descriptor_set(m_device, m_descriptor_pool, m_descriptor_set_layout, m_uniform_buffers[i].allocated_buffer.buffer, m_texture.view, m_texture_sampler);

			m_cmd_buffers[i] = create_command_buffer(m_device, m_cmd_pool);
		}

		// Synchronization
		create_sync_objects();



		m_frag_shader_watcher = std::make_unique<ShaderFileWatcher>(ShaderFileWatcher(std::string(WORKSPACE_DIR) + "/shaders/frag.frag"));
		m_vert_shader_watcher = std::make_unique<ShaderFileWatcher>(ShaderFileWatcher(std::string(WORKSPACE_DIR) + "/shaders/vert.vert"));

		// noesis init

		m_render_pass_ns = UtilsNs::CreateRenderPass(m_device, VK_SAMPLE_COUNT_1_BIT, true, m_swapchain_image_format, m_depth_stencil.image_info.format);
#define NS_DESCRIPTOR_POOL_MAX_SETS 128
		UtilsNs::CreateNoesisDescriptorLayout(m_device, m_noesis);
		std::vector<VkDescriptorPoolSize>pool_sizes_ns =
		{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, NS_DESCRIPTOR_POOL_MAX_SETS * 4},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, NS_DESCRIPTOR_POOL_MAX_SETS * 3} };

		m_descriptor_pool_ns = create_descriptor_pool(m_device, pool_sizes_ns);

		Noesis::SetLogHandler([](const char*, uint32_t, uint32_t level, const char*, const char* msg)
			{
				// [TRACE] [DEBUG] [INFO] [WARNING] [ERROR]
				const char* prefixes[] = { "T", "D", "I", "W", "E" };
				printf("[NOESIS/%s] %s\n", prefixes[level], msg);
			});
		Noesis::GUI::SetLicense(NS_LICENSE_NAME, NS_LICENSE_KEY);

		Noesis::GUI::Init();

		// You need a view to render the user interface and interact with it. A view holds a tree of
		// elements. The easiest way to construct a tree is from a XAML file
		const char* xaml = R"(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
  <TextBlock Text="Hello, Noesis!" />
</Grid>
)";
		Noesis::Ptr<Noesis::FrameworkElement> root = Noesis::GUI::ParseXaml<Noesis::FrameworkElement>(xaml);


		// You need a view to render the user interface and interact with it. A view holds a tree of
	// elements. The easiest way to construct a tree is from a XAML file
		m_view_ns = Noesis::GUI::CreateView(root);
		m_view_ns->SetSize(720, 480);

		// As we are not using MSAA in this sample, we enable PPAA here. PPAA is a cheap antialising
		// technique that extrudes the contours of the geometry smoothing them
		m_view_ns->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
		m_view_ns->GetRenderer()->Init(this);

		m_ready = true;
	}

	std::unique_ptr<Model> RendererVk::load_model(std::string dir)
	{
		std::unique_ptr<Model> model = Model::import_model(dir, m_all_vertices, m_all_indices);
		return model;
	}

	void RendererVk::cleanup_swapchain()
	{
		// Destroy depth buffer
		vkDestroyImageView(m_device, m_depth_stencil.view, nullptr);
		vmaDestroyImage(m_allocator, m_depth_stencil.image, m_depth_stencil.allocation);

		for (auto framebuffer : m_swapchain_framebuffers)
		{
			vkDestroyFramebuffer(m_device, framebuffer, nullptr);
		}

		for (auto imageView : m_swapchain_image_views)
		{
			vkDestroyImageView(m_device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}

	RendererVk::~RendererVk()
	{
		vkDeviceWaitIdle(m_device);

		vkDestroyPipelineLayout(m_device, m_noesis.pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_noesis.descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(m_device, m_descriptor_pool_ns, nullptr);

		// Destroy synchronization objects
		for (size_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i)
		{
			vkDestroySemaphore(m_device, m_available_image_semaphores[i], nullptr);
			vkDestroySemaphore(m_device, m_finished_render_semaphores[i], nullptr);
			vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
		}

		vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);
		// Destroy descriptor pool
		vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

		// Destroy uniform buffers
		for (auto& ub : m_uniform_buffers)
		{
			vkDestroyBuffer(m_device, ub.allocated_buffer.buffer, nullptr);
			vmaUnmapMemory(m_allocator, ub.allocated_buffer.allocation);
			vmaFreeMemory(m_allocator, ub.allocated_buffer.allocation);
		}

		// Destroy vertex and index buffer
		vkDestroyBuffer(m_device, m_geometry_buffer.indices.buffer, nullptr);
		vmaFreeMemory(m_allocator, m_geometry_buffer.indices.allocation);
		vkDestroyBuffer(m_device, m_geometry_buffer.vertices.buffer, nullptr);
		vmaFreeMemory(m_allocator, m_geometry_buffer.vertices.allocation);

		// Destroy pipeline and related layouts
		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
		vkDestroyRenderPass(m_device, m_render_pass, nullptr);

		// Destroy texture resources
		vkDestroySampler(m_device, m_texture_sampler, nullptr);
		vkDestroyImageView(m_device, m_texture.view, nullptr);
		vkDestroyImage(m_device, m_texture.image, nullptr);
		vmaFreeMemory(m_allocator, m_texture.allocation);

		// Destroy command pool
		vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);

		cleanup_swapchain();

		vmaDestroyAllocator(m_allocator);

		// Destroy device, surface, instance
		vkDestroyDevice(m_device, nullptr);

		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

		vkDestroyInstance(m_instance, nullptr);

		m_window = nullptr;
	}

	void RendererVk::create_instance()
	{
		auto instance_extensions = ToolsVk::get_required_instance_extensions(m_enable_validation_layers);

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

		if (m_enable_validation_layers)
		{
			ToolsVk::populate_debug_messenger_create_info(debug_create_info);
			create_info.enabledLayerCount = m_validation_layers.size();
			create_info.ppEnabledLayerNames = m_validation_layers.data();
			create_info.pNext = &debug_create_info;
		}

		VK_CHECK_RESULT(vkCreateInstance(&create_info, nullptr, &m_instance));
	}

	void RendererVk::create_surface()
	{
		auto err = SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface);
		// Create a Vulkan surface using SDL
		if (!err)
		{
			// Handle surface creation error
			throw std::runtime_error("Failed to create Vulkan surface");
		}
	}

	void RendererVk::select_physical_device()
	{

		uint32_t gpu_count = 0;
		std::vector<VkPhysicalDevice> gpus;
		vkEnumeratePhysicalDevices(m_instance, &gpu_count, nullptr);
		gpus.resize(gpu_count);
		vkEnumeratePhysicalDevices(m_instance, &gpu_count, gpus.data());

		for (auto& gpu : gpus)
		{

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

	void RendererVk::create_logical_device_and_queues()
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
		requiredFeatures.fillModeNonSolid = VK_TRUE;

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

	void RendererVk::create_swapchain()
	{
		ToolsVk::SwapChainSupportDetails swapchain_support_details = ToolsVk::query_swap_chain_support(m_chosen_phys_device, m_surface);

		m_surface_format = ToolsVk::choose_swap_surface_format(swapchain_support_details.formats);
		// VkExtent2D extent = tools::choose_swap_extent(swapchain_support_details.capabilities, m_window);

		// uint32_t image_count = swapchain_support_details.capabilities.minImageCount + 1;
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
		create_info.imageExtent = m_extent;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		ToolsVk::QueueFamilyIndices indices = ToolsVk::findQueueFamilies(m_chosen_phys_device, m_surface);
		uint32_t queue_family_indices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily)
		{
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = 2;
			create_info.pQueueFamilyIndices = queue_family_indices;
		}
		else
		{
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		create_info.preTransform = swapchain_support_details.capabilities.currentTransform;
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create_info.presentMode = PRESENT_MODE;
		create_info.clipped = VK_TRUE;

		VK_CHECK_RESULT(vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain));

		vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
		m_swapchain_images.resize(image_count);
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_swapchain_images.data());

		m_swapchain_image_format = m_surface_format.format;
	}

	VkImageView RendererVk::create_swapchain_image_views(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags flags)
	{
		VkImageView image_view;

		image_view = ToolsVk::create_image_view(m_device, image,
			m_swapchain_image_format, flags);

		return image_view;
	}

	void RendererVk::create_geometry_buffer()
	{
		VkDeviceSize vertex_buffer_size = sizeof(Vertex) * m_all_vertices.size();
		VkDeviceSize index_buffer_size = sizeof(uint32_t) * m_all_indices.size();

		m_geometry_buffer.index_count = m_all_indices.size();
		m_geometry_buffer.vertex_count = m_all_vertices.size();

		// === STAGING BUFFERS ===

		AllocatedBuffer vertex_staging = ToolsVk::create_buffer(m_allocator, vertex_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VmaAllocationCreateInfo staging_alloc_info = {};
		staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

		void* data;
		VK_CHECK_RESULT(vmaMapMemory(m_allocator, vertex_staging.allocation, &data));
		memcpy(data, m_all_vertices.data(), static_cast<size_t>(vertex_buffer_size));
		vmaUnmapMemory(m_allocator, vertex_staging.allocation);

		AllocatedBuffer index_staging = ToolsVk::create_buffer(m_allocator, index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VK_CHECK_RESULT(vmaMapMemory(m_allocator, index_staging.allocation, &data));
		memcpy(data, m_all_indices.data(), static_cast<size_t>(index_buffer_size));
		vmaUnmapMemory(m_allocator, index_staging.allocation);

		// === DEVICE LOCAL BUFFERS ===
		VmaAllocationCreateInfo device_alloc_info = {};
		device_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		// Vertex buffer (device-local)
		m_geometry_buffer.vertices = ToolsVk::create_buffer(m_allocator, vertex_buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		// Index buffer (device-local)
		m_geometry_buffer.indices = ToolsVk::create_buffer(m_allocator, index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		// === COPY ===
		ToolsVk::copy_buffer(m_device, m_cmd_pool, m_graphics_queue, vertex_staging.buffer, m_geometry_buffer.vertices.buffer, vertex_buffer_size);
		ToolsVk::copy_buffer(m_device, m_cmd_pool, m_graphics_queue, index_staging.buffer, m_geometry_buffer.indices.buffer, index_buffer_size);

		// Cleanup staging
		vmaDestroyBuffer(m_allocator, vertex_staging.buffer, vertex_staging.allocation);
		vmaDestroyBuffer(m_allocator, index_staging.buffer, index_staging.allocation);
	}

	VkRenderPass RendererVk::create_renderpass(VkDevice device, VkFormat color_format, VkFormat depth_format, bool is_presenting_pass)
	{

		// This function will prepare a single render pass with one subpass

		// Descriptors for render pass attachments
		// Color attachment
		// attachments[0].flags = 0,
		VkAttachmentDescription depth_attachment_desc{};
		depth_attachment_desc.format = color_format;
		depth_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		if (is_presenting_pass) {
			depth_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}
		// Depth attachment

		// attachments[1].flags = 0;
		VkAttachmentDescription color_attach_desc{};

		color_attach_desc.format = depth_format;
		color_attach_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attach_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;						   // Clear depth at start of first subpass
		color_attach_desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;					   // We don't need depth after render pass has finished (DONT_CARE may result in better performance)
		color_attach_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;				   // No stencil
		color_attach_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;			   // No Stencil
		color_attach_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					   // Layout at render pass start. Initial doesn't matter, so we use undefined
		color_attach_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Transition to depth/stencil attachment

		std::vector<VkAttachmentDescription> attachments{ depth_attachment_desc, color_attach_desc };


		VkAttachmentReference color_ref = {};
		color_ref.attachment = 0;
		color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_ref = {};
		depth_ref.attachment = 1;
		depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Geometry subpass
		std::array<VkSubpassDescription, 1> subpasses{};
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].colorAttachmentCount = 1;
		subpasses[0].pColorAttachments = &color_ref;
		subpasses[0].pDepthStencilAttachment = &depth_ref;

		std::array<VkSubpassDependency, 2> dependencies{};
		// 1) External -> Subpass 0: sync clears (color + depth)
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// 2) Subpass 0 -> External: sync store + final transition
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		render_pass_info.pAttachments = attachments.data();
		render_pass_info.subpassCount = static_cast<uint32_t>(subpasses.size());
		render_pass_info.pSubpasses = subpasses.data();
		render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
		render_pass_info.pDependencies = dependencies.data();

		VkResult err;
		VkRenderPass render_pass;
		err = vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass);
		assert(!err);

		VK_CHECK_RESULT(err);
		ToolsVk::set_object_name(device, (uint64_t)render_pass, VK_OBJECT_TYPE_RENDER_PASS, "MainRenderpass");
		return render_pass;
	}

	VkPipeline RendererVk::create_pipeline(VkDevice device, VkRenderPass renderpass, VkPipelineLayout pipeline_layout)
	{
		VkShaderModule vert_shader_module = ToolsVk::createShaderModule(device, (WORKSPACE_DIR + std::string("/shaders/vert.spv")));
		VkShaderModule frag_shader_module = ToolsVk::createShaderModule(device, (WORKSPACE_DIR + std::string("/shaders/frag.spv")));

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
		pipeline_info.layout = pipeline_layout;
		pipeline_info.renderPass = renderpass;
		pipeline_info.subpass = 0;
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

		vkDestroyShaderModule(device, frag_shader_module, nullptr);
		vkDestroyShaderModule(device, vert_shader_module, nullptr);

		return pipeline;
	}

	VkCommandPool RendererVk::create_command_pool(VkDevice device, uint32_t graphics_queue_family_index)
	{

		VkCommandPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = graphics_queue_family_index;

		VkCommandPool command_pool{};
		VK_CHECK_RESULT(vkCreateCommandPool(device, &pool_info, nullptr, &command_pool));
		return command_pool;
	}

	VkCommandBuffer RendererVk::create_command_buffer(VkDevice device, VkCommandPool command_pool)
	{
		VkCommandBufferAllocateInfo cmd_buf_info{};
		cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_buf_info.commandPool = command_pool;
		cmd_buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_buf_info.commandBufferCount = 1;

		VkCommandBuffer command_buffer{};
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmd_buf_info, &command_buffer));
		return command_buffer;
	}

	VkDescriptorPool RendererVk::create_descriptor_pool(VkDevice device, std::vector<VkDescriptorPoolSize> pool_sizes) {

		VkDescriptorPoolCreateInfo pool_info{};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.pNext = nullptr;
		pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
		pool_info.pPoolSizes = pool_sizes.data();
		pool_info.maxSets = static_cast<uint32_t>(MAX_CONCURRENT_FRAMES);

		VkDescriptorPool pool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &pool));
		return pool;

	}

	VkDescriptorSet RendererVk::create_descriptor_set(VkDevice device, VkDescriptorPool descriptor_pool,
		VkDescriptorSetLayout descriptor_set_layout, VkBuffer buffer, VkImageView image_view,
		VkSampler sampler)
	{

		VkDescriptorSet descriptor_set;

		VkDescriptorSetAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &descriptor_set_layout;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device,
			&alloc_info,
			&descriptor_set));

		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = buffer;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(MVP_uniform_object);

		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = image_view;
		image_info.sampler = sampler;

		// VkWriteDescriptorSet - Represents a descriptor set write operation 
		std::array<VkWriteDescriptorSet, 2> descriptor_writes{};
		descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[0].dstSet = descriptor_set;
		descriptor_writes[0].dstBinding = 0;
		descriptor_writes[0].dstArrayElement = 0;
		descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].pBufferInfo = &buffer_info;

		descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[1].dstSet = descriptor_set;
		descriptor_writes[1].dstBinding = 1;
		descriptor_writes[1].dstArrayElement = 0;
		descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].pImageInfo = &image_info;

		vkUpdateDescriptorSets(device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

		return descriptor_set;
	}

	VkFramebuffer RendererVk::create_framebuffer(VkDevice device, VkImageView view, VkImageView depth_view)
	{
		VkResult err;
		VkFramebuffer framebuffer;
		std::vector<VkImageView> attachments{ view };

		if (depth_view != VK_NULL_HANDLE) {
			attachments.push_back(depth_view);
		}

		// All framebuffers use same renderpass setup
		VkFramebufferCreateInfo framebuffer_info{};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = m_render_pass;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = m_extent.width;
		framebuffer_info.height = m_extent.height;
		framebuffer_info.layers = 1;

		// Create framebuffer
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffer));
		return framebuffer;
	}

	void RendererVk::create_sync_objects()
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

	void RendererVk::record_draw_commands(VkCommandBuffer command_buffer, uint32_t image_index)
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
		renderpass_info.renderArea.extent.width = m_extent.width;
		renderpass_info.renderArea.extent.height = m_extent.height;
		renderpass_info.clearValueCount = 2;
		renderpass_info.pClearValues = clear_col.data();

		vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)m_extent.width;
		viewport.height = (float)m_extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(command_buffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = m_extent.width;
		scissor.extent.height = m_extent.height;

		vkCmdSetScissor(command_buffer, 0, 1, &scissor);

		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &m_geometry_buffer.vertices.buffer, offsets);

		vkCmdBindIndexBuffer(command_buffer, m_geometry_buffer.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_pipeline_layout, 0, 1,
			&m_uniform_buffers[m_current_frame].descriptorSet,
			0, nullptr);

		vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(m_geometry_buffer.index_count), 1, 0, 0, 0);

		vkCmdEndRenderPass(command_buffer);

		auto* r = m_view_ns->GetRenderer();
		r->UpdateRenderTree();
		r->Render();

		//renderpass_info = {};
		//renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		//renderpass_info.pNext = nullptr;
		//renderpass_info.renderPass = m_render_pass_ns;
		//renderpass_info.framebuffer = m_noesis.fb;
		//renderpass_info.renderArea.offset.x = 0;
		//renderpass_info.renderArea.offset.y = 0;
		//renderpass_info.renderArea.extent.width = m_extent.width;
		//renderpass_info.renderArea.extent.height = m_extent.height;
		//renderpass_info.clearValueCount = 2;
		//clear_col[0] = { 1.0, 1.0, 1.0, 1.0 };
		//renderpass_info.pClearValues = clear_col.data();

		VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));
	}

	void RendererVk::draw_frame()
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
		record_draw_commands(m_cmd_buffers[m_current_frame], image_index);

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

	VkDescriptorSetLayout RendererVk::create_descriptor_set_layout(const std::vector<VkDescriptorSetLayoutBinding>& layout_bindings)
	{

		VkDescriptorSetLayoutCreateInfo layout_info{};
		layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
		layout_info.pBindings = layout_bindings.data();
		VkDescriptorSetLayout layout;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &layout));
		return layout;
	}

	VkPipelineLayout RendererVk::create_pipeline_layout(VkDevice device, VkDescriptorSetLayout descriptor_set_layout) {
		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.pNext = nullptr;
		pipeline_layout_info.setLayoutCount = 1;
		// pipeline_layout_info.pushConstantRangeCount = 0;
		pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

		VkPipelineLayout pipeline_layout{};
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));
		return pipeline_layout;
	}

	UniformBuffer RendererVk::create_uniform_buffer(VmaAllocator allocator, VkDeviceSize buffer_size)
	{
		UniformBuffer uniform_buffer;
		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = buffer_size;
		buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Host-visible and coherent by default


		uniform_buffer.allocated_buffer = ToolsVk::create_buffer(allocator,
			buffer_size,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			// Map once for persistent updates
			VK_CHECK_RESULT(vmaMapMemory(
			allocator,
			uniform_buffer.allocated_buffer.allocation,
			reinterpret_cast<void**>(&uniform_buffer.mapped)));

		return uniform_buffer;
		}

	void RendererVk::update_uniform_buffer()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		glm::vec3 start{ 0.0f, 3.0f, -2.0f };
		glm::vec3 end{ 0.0f, 3.0f, -5.0f };
		float t = sin(time / 4.0f * glm::pi<float>()) * 0.5f + 0.5f;

		// glm::vec3 camera_pos = (1.0f - t) * start + t * end;

		MVP_uniform_object ubo{};
		// ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.model = glm::mat4(1.0f);
		ubo.view = glm::lookAt(m_camera.pos, m_camera.forward_dir, glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(m_extent.width) / m_extent.height, 0.1f, 1000.0f);

		ubo.projection[1][1] *= -1;

		memcpy(m_uniform_buffers[m_current_frame].mapped, &ubo, sizeof(ubo));
	}


	bool RendererVk::isReady() { return m_ready; };


	void RendererVk::update(uint64_t delta_t)
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

		// Check if shader files have changed
		const bool frag_shader_changed = m_frag_shader_watcher->check_for_changes();
		const bool vert_shader_changed = m_vert_shader_watcher->check_for_changes();
		if (frag_shader_changed || vert_shader_changed)
		{
			m_pipeline = create_pipeline(m_device, m_render_pass, m_pipeline_layout);
		}

		// if (m_ui_renderer) {
		//	m_ui_renderer->Update(Time::Instance().RunningTimeSeconds());
		// }
	}
	void RendererVk::on_input_event(const SDL_Event& event)
	{

		// Check for up keys
		if (event.type == SDL_EVENT_KEY_UP)
		{
			switch (event.key.key)
			{
			case SDLK_W:
				m_camera.moveForward = false;
				break;
			case SDLK_S:
				m_camera.moveBack = false;
				break;
			case SDLK_A:
				m_camera.moveLeft = false;
				break;
			case SDLK_D:
				m_camera.moveRight = false;
				break;
			}
		}

		// Check for down keys
		if (event.type == SDL_EVENT_KEY_DOWN)
		{
			switch (event.key.key)
			{
			case SDLK_W:
				m_camera.moveForward = true;
				break;
			case SDLK_S:
				m_camera.moveBack = true;
				break;
			case SDLK_A:
				m_camera.moveLeft = true;
				break;
			case SDLK_D:
				m_camera.moveRight = true;
				break;
			}
		}
	}

	void RendererVk::create_memory_allocator()
	{
		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;
		allocatorCreateInfo.physicalDevice = m_chosen_phys_device;
		allocatorCreateInfo.device = m_device;
		allocatorCreateInfo.instance = m_instance;

		vmaCreateAllocator(&allocatorCreateInfo, &m_allocator);
	}

	const Noesis::DeviceCaps& RendererVk::GetCaps() const
	{
		static Noesis::DeviceCaps caps = {};

		// linearRendering: true if swapchain is SRGB format
		caps.linearRendering = (m_swapchain_image_format == VK_FORMAT_R8G8B8A8_SRGB ||
			m_swapchain_image_format == VK_FORMAT_B8G8R8A8_SRGB);
		caps.clipSpaceYInverted = false;
		caps.centerPixelOffset = 0.0;

		return caps;
	}

	class VKTexture : public Noesis::Texture
	{
	public:
		VkImage image;
		VkImageView view;
		VmaAllocation allocation;
		VkImageCreateInfo image_info;
		VkImageViewCreateInfo view_info;
		VkImageLayout layout;
		VKTexture() {}

		~VKTexture()
		{

		}

		uint32_t GetWidth() const override { return image_info.extent.width; }
		uint32_t GetHeight() const override { return image_info.extent.height; }
		bool HasMipMaps() const override { return image_info.mipLevels > 1; }
		bool IsInverted() const override { return false; }
		bool HasAlpha() const override { return true; }


		//VKRenderDevice* device = VK_NULL_HANDLE;

		//VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		//VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

	};

	//Noesis::Ptr<Noesis::Texture> RendererVk::WrapTexture(VkImage image, uint32_t width, uint32_t height,
	//	uint32_t levels, VkFormat format, VkImageLayout layout, bool isInverted, bool hasAlpha)
	//{
	//	Noesis::Ptr<VKTexture> texture = Noesis::MakePtr<VKTexture>();

	//	texture->image = image;
	//	texture->format = format;
	//	texture->layout = layout;
	//	texture->image_info.extent = { width, height, 1 };
	//	texture->image_info.mipLevels = levels;
	//	return texture;
	//}


	class VKRenderTarget final : public Noesis::RenderTarget
	{
	public:
		~VKRenderTarget()
		{
		}

		Noesis::Texture* GetTexture() override { return color; }

		Noesis::Ptr<VKTexture> color;
		Noesis::Ptr<VKTexture> colorAA;
		Noesis::Ptr<VKTexture> stencil;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	};


	Noesis::Ptr<Noesis::RenderTarget> RendererVk::CreateRenderTarget(const char* label, uint32_t width, uint32_t height, uint32_t sampleCount, bool needsStencil)
	{

		using Noesis::MakePtr;

		auto surface = MakePtr<VKRenderTarget>();
		surface->samples = VK_SAMPLE_COUNT_1_BIT;

		const VkFormat colorFmt = m_swapchain_image_format;

		// 1) Single-sample color (attachment + sampled)
		auto colorTex = TextureVk::create_texture(
			m_device, m_cmd_pool, m_graphics_queue, m_allocator,
			/*pixelData*/ nullptr,
			width, height,
			/*mip_levels*/ 1,
			/*format*/ colorFmt,
			/*aspect*/ VK_IMAGE_ASPECT_COLOR_BIT,
			/*extra_usage*/ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			/*final_layout*/ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		auto color = Noesis::MakePtr<VKTexture>();
		color->image = colorTex.image;
		color->view = colorTex.view;
		color->allocation = colorTex.allocation;
		color->image_info = colorTex.image_info;
		color->view_info = colorTex.view_info;
		color->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		color->image_info.extent = { width, height };
		surface->color = color;  // This is the one Noesis will sample

		// 2) Optional depth/stencil
		std::vector<VkImageView> atts;
		atts.reserve(2);
		atts.push_back(surface->color->view);

		VkFormat dsFmt = VK_FORMAT_UNDEFINED;
		if (needsStencil) {
			dsFmt = ToolsVk::find_supported_format(
				m_chosen_phys_device,
				{ VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT },
				VK_IMAGE_TILING_OPTIMAL,
				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

			auto dsTex = TextureVk::create_texture(
				m_device, m_cmd_pool, m_graphics_queue, m_allocator,
				/*pixelData*/ nullptr,
				width, height,
				/*mip_levels*/ 1,
				/*format*/ dsFmt,
				/*aspect*/ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
				/*extra_usage*/ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				/*final_layout*/ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

			auto ds = Noesis::MakePtr<VKTexture>();
			ds->image = dsTex.image;
			ds->view = dsTex.view;
			ds->allocation = dsTex.allocation;
			ds->image_info = dsTex.image_info;
			ds->view_info = dsTex.view_info;
			ds->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			ds->image_info.extent = { width, height };
			surface->stencil = ds;

			atts.push_back(surface->stencil->view);
		}

		// 3) Render pass (single-sample, no resolve)
		VkAttachmentDescription colorAtt{};
		colorAtt.format = colorFmt;
		colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentDescription dsAtt{};
		if (needsStencil) {
			dsAtt.format = dsFmt;
			dsAtt.samples = VK_SAMPLE_COUNT_1_BIT;
			dsAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			dsAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			dsAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			dsAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			dsAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			dsAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference dsRef{};
		if (needsStencil) {
			dsRef.attachment = 1;
			dsRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkSubpassDescription sub{};
		sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sub.colorAttachmentCount = 1;
		sub.pColorAttachments = &colorRef;
		sub.pDepthStencilAttachment = needsStencil ? &dsRef : nullptr;

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = 0;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachments[2];
		uint32_t attachmentCount = 1;
		attachments[0] = colorAtt;
		if (needsStencil) {
			attachments[1] = dsAtt;
			attachmentCount = 2;
		}

		VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		rpci.attachmentCount = attachmentCount;
		rpci.pAttachments = attachments;
		rpci.subpassCount = 1;
		rpci.pSubpasses = &sub;
		rpci.dependencyCount = 1;
		rpci.pDependencies = &dep;

		VK_CHECK_RESULT(vkCreateRenderPass(m_device, &rpci, nullptr, &surface->renderPass));

		// 4) Framebuffer
		VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbci.renderPass = surface->renderPass;
		fbci.attachmentCount = static_cast<uint32_t>(atts.size());
		fbci.pAttachments = atts.data();
		fbci.width = width;
		fbci.height = height;
		fbci.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &fbci, nullptr, &surface->framebuffer));

		return surface;

	}

	Noesis::Ptr<Noesis::RenderTarget> RendererVk::CloneRenderTarget(const char* label, Noesis::RenderTarget* surface_)
	{
		using namespace Noesis;

		VKRenderTarget* src = (VKRenderTarget*)surface_;

		uint32_t width = src->color->GetWidth();
		uint32_t height = src->color->GetHeight();

		Ptr<VKRenderTarget> surface = MakePtr<VKRenderTarget>();
		surface->renderPass = src->renderPass;
		surface->samples = src->samples;
		surface->colorAA = src->colorAA;
		surface->stencil = src->stencil;

		Vector<VkImageView, 3> attachments;
		if (surface->stencil)
		{
			attachments.PushBack(surface->stencil->view);
		}

		//EnsureTransferCommands();

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = surface->renderPass;
		framebufferInfo.attachmentCount = attachments.Size();
		framebufferInfo.pAttachments = attachments.Data();
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &surface->framebuffer));
		VK_NAME(surface->framebuffer, FRAMEBUFFER, "Noesis_%s_FrameBuffer", label);
		return surface;
	}


	Noesis::Ptr<Noesis::Texture> RendererVk::CreateTexture(const char* label, uint32_t width, uint32_t height,
		uint32_t numLevels, Noesis::TextureFormat::Enum format, const void** data)
	{
		// covert noesis format to vulkan foramt
		VkFormat vk_format = VK_FORMAT_R8G8B8A8_SRGB;
		switch (format) {
		case Noesis::TextureFormat::R8:
			vk_format = VK_FORMAT_R8_UNORM;
			break;
		case Noesis::TextureFormat::RGBA8:
			vk_format = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		default:
			assert(false);
			break;
		}

		TextureVk tex = TextureVk::create_texture(m_device, m_cmd_pool, m_graphics_queue, m_allocator, data, width, height, numLevels);
		Noesis::Ptr<VKTexture> texture = Noesis::MakePtr<VKTexture>();
		texture->image = tex.image;
		texture->image_info = tex.image_info;
		texture->allocation = tex.allocation;
		texture->view = tex.view;
		texture->view_info = tex.view_info;
		return texture;
	}

	void RendererVk::UpdateTexture(Noesis::Texture* texture_, uint32_t level, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data)
	{

		VKTexture* texture = (VKTexture*)texture_;

		uint32_t texel_size = texture->image_info.format == VK_FORMAT_R8_UNORM ? 1 : 4;
		uint32_t size = width * height * texel_size;
		AllocatedBuffer staging_buffer = ToolsVk::create_buffer(m_allocator, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// Copy data into staging buffer
		void* mapped;
		VK_CHECK_RESULT(vmaMapMemory(m_allocator, staging_buffer.allocation, &mapped));
		memcpy(mapped, data, static_cast<size_t>(size));
		vmaUnmapMemory(m_allocator, staging_buffer.allocation);

		TextureVk::transition_image_layout(m_device, m_cmd_pool, m_graphics_queue, texture->image, texture->image_info.format, texture->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy region{};
		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageExtent.width = width;
		region.imageExtent.height = height;
		region.imageExtent.depth = 1;
		region.imageSubresource.mipLevel = level;
		region.bufferOffset = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkCommandBuffer cmd = ToolsVk::begin_single_time_commands(m_device, m_cmd_pool);
		vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		ToolsVk::end_single_time_commands(m_device, m_cmd_pool, cmd, m_graphics_queue);

		// Transition back for shader access
		TextureVk::transition_image_layout(m_device, m_cmd_pool, m_graphics_queue,
			texture->image, texture->image_info.format,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			texture->layout);

		vmaDestroyBuffer(m_allocator, staging_buffer.buffer, staging_buffer.allocation);
	}

	void RendererVk::BeginOffscreenRender()
	{
		// NA
	}
	void RendererVk::EndOffscreenRender()
	{
		// NA
	}
	void RendererVk::BeginOnscreenRender()
	{
		// NA
	}
	void RendererVk::EndOnscreenRender()
	{
		// NA
	}
	void RendererVk::SetRenderTarget(Noesis::RenderTarget* surface_)
	{
		VKRenderTarget* surface = (VKRenderTarget*)surface_;
		VK_BEGIN_EVENT("SetRenderTarget");

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)surface->color->GetWidth();
		viewport.height = (float)surface->color->GetHeight();
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdSetViewport(m_cmd_buffers[m_current_frame], 0, 1, &viewport);
	}

	void RendererVk::BeginTile(Noesis::RenderTarget* surface, const Noesis::Tile& tile)
	{
	}
	void RendererVk::EndTile(Noesis::RenderTarget* surface)
	{
	}
	void RendererVk::ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles, uint32_t numTiles)
	{
		// Not supporting msaa yet
		// NA
	}
	void* RendererVk::MapVertices(uint32_t bytes)
	{

		// Toss any previous staging from the last frame
		if (m_vertex_staging_ns.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_vertex_staging_ns.buffer, m_vertex_staging_ns.allocation);
			m_vertex_staging_ns = {};
		}

		if (bytes == 0) {
			m_noesis.iboWriteOffset = 0;
			return nullptr;
		}

		m_noesis.vboWriteOffset = bytes;
		// allocate memory
		m_vertex_staging_ns = ToolsVk::create_buffer(m_allocator, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// map memory
		void* data;
		if (bytes) {
			VK_CHECK_RESULT(vmaMapMemory(m_allocator, m_vertex_staging_ns.allocation, &data));
		}
		return data;
	}

	void RendererVk::UnmapVertices()
	{
		if (m_vertex_staging_ns.buffer == VK_NULL_HANDLE) return;

		vmaUnmapMemory(m_allocator, m_vertex_staging_ns.allocation);

		// Recreate a tight device-local VBO sized to the written bytes
		if (m_noesis.vbo.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_noesis.vbo.buffer, m_noesis.vbo.allocation);
			m_noesis.vbo = {};
	}

		if (m_noesis.vboWriteOffset) {
			m_noesis.vbo = ToolsVk::create_buffer(
				m_allocator,
				m_noesis.vboWriteOffset,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			ToolsVk::copy_buffer(
				m_device, m_cmd_pool, m_graphics_queue,
				m_vertex_staging_ns.buffer, m_noesis.vbo.buffer,
				m_noesis.vboWriteOffset);
		}

		vmaDestroyBuffer(m_allocator, m_vertex_staging_ns.buffer, m_vertex_staging_ns.allocation);
		m_vertex_staging_ns = {};
		m_noesis.vboWriteOffset = 0;

	}
	void* RendererVk::MapIndices(uint32_t bytes)
	{

		// Toss any previous staging from the last frame
		if (m_index_staging_ns.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_index_staging_ns.buffer, m_index_staging_ns.allocation);
			m_index_staging_ns = {};
		}


		if (bytes == 0) {
			m_noesis.iboWriteOffset = 0;
			return nullptr;
		}

		m_noesis.iboWriteOffset = bytes;
		// allocate memory
		m_index_staging_ns = ToolsVk::create_buffer(m_allocator, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_ONLY, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		// map memory
		void* data;
		if (bytes) {
			VK_CHECK_RESULT(vmaMapMemory(m_allocator, m_index_staging_ns.allocation, &data));
		}
		return data;
	}
	void RendererVk::UnmapIndices()
	{
		if (m_index_staging_ns.buffer == VK_NULL_HANDLE) return;

		vmaUnmapMemory(m_allocator, m_index_staging_ns.allocation);

		// Recreate a tight device-local IBO sized to the written bytes
		if (m_noesis.ibo.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, m_noesis.ibo.buffer, m_noesis.ibo.allocation);
			m_noesis.ibo = {};
	}

		if (m_noesis.iboWriteOffset) {
			m_noesis.ibo = ToolsVk::create_buffer(
				m_allocator,
				m_noesis.iboWriteOffset,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			ToolsVk::copy_buffer(
				m_device, m_cmd_pool, m_graphics_queue,
				m_index_staging_ns.buffer, m_noesis.ibo.buffer,
				m_noesis.iboWriteOffset);
		}

		vmaDestroyBuffer(m_allocator, m_index_staging_ns.buffer, m_index_staging_ns.allocation);
		m_index_staging_ns = {};
		m_noesis.iboWriteOffset = 0;
	}

	void RendererVk::DrawBatch(const Noesis::Batch& batch)
	{
		// If uploads didn't happen yet, skip safely
		if (m_noesis.vbo.buffer == VK_NULL_HANDLE || m_noesis.ibo.buffer == VK_NULL_HANDLE)
			return;

		VkCommandBuffer cmd = m_cmd_buffers[m_current_frame];

		// Bind VB/IB at offset 0 (we re-created them per Unmap*)
		const VkBuffer vbs[] = { m_noesis.vbo.buffer };
		const VkDeviceSize vboOffsets[] = { 0 };
		vkCmdBindVertexBuffers(cmd, 0, 1, vbs, vboOffsets);

		// Noesis indices are 16-bit
		vkCmdBindIndexBuffer(cmd, m_noesis.ibo.buffer, 0, VK_INDEX_TYPE_UINT16);

		const uint32_t firstIndex = batch.startIndex;   // base is 0 with this simple path
		const uint32_t indexCount = batch.numIndices;

		// NOTE: pipeline & descriptors are purposefully not (re)bound here.
		// Bind whatever pipeline/sets you want before calling Noesis to render.
		vkCmdDrawIndexed(cmd, indexCount, 1, firstIndex, 0, 0);
	}


}