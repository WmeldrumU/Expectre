// Library macros
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define STB_IMAGE_IMPLEMENTATION // includes stb function bodies

// Renderer macros
#define RESOLUTION_X 640
#define RESOLUTION_Y 480

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

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "rendererVk.h"
#include "shared.h"
#include "spdlog/spdlog.h"
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
			RESOLUTION_X, RESOLUTION_Y,
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

		create_command_buffers();

		create_swapchain();

		create_texture_image();

		create_texture_image_view();

		create_texture_sampler();

		prepare_depth();

		create_renderpass();

		create_uniform_buffers();

		create_descriptor_set_layout();

		create_descriptor_pool_and_sets();

		create_pipeline();

		create_framebuffers();

		create_vertex_buffer();

		create_uniform_buffers();

		create_descriptor_pool_and_sets();

		create_sync_objects();

		m_ready = true;
	}

	Renderer_Vk::~Renderer_Vk()
	{
		vkDeviceWaitIdle(m_device);

		vkDestroySampler(m_device, m_texture_sampler, nullptr);
		vkDestroyImageView(m_device, m_texture_image_view, nullptr);
		vkDestroyImage(m_device, m_texture_image, nullptr);
		vkFreeMemory(m_device, m_texture_image_memory, nullptr);

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
		vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);

		// Destroy depth buffer
		vkDestroyImageView(m_device, m_depth.view, nullptr);
		vkDestroyImage(m_device, m_depth.image, nullptr);
		vkFreeMemory(m_device, m_depth.mem, nullptr);

		vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyImageView(m_device, m_image_view, nullptr);
		vkDestroyImage(m_device, m_image, nullptr);
		vkDestroyDevice(m_device, nullptr);
		vkDestroyInstance(m_instance, nullptr);
		SDL_DestroyWindow(m_window);
		SDL_Vulkan_UnloadLibrary();
		SDL_Quit();
		SDL_Log("Cleaned up with errors: %s", SDL_GetError());

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
		std::vector<const char*> extensions(num_extensions);
		if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &num_extensions, extensions.data()))
		{
			SDL_Log("Unable to get vulkan extensions: %s", SDL_GetError());
			throw std::runtime_error("Unable to get vulkan extension names:");
		}

		// print extensions
		for (const char* ext : extensions)
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
		const char** p_layers = nullptr;
		if (m_layers_supported)
		{
			layer_count = static_cast<uint32_t>(m_layers.size());
			p_layers = m_layers.data();
		}
		instance_create_info.enabledLayerCount = layer_count;
		instance_create_info.ppEnabledLayerNames = p_layers;

		// Extensions
		instance_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instance_create_info.ppEnabledExtensionNames = extensions.data();

		auto result = vkCreateInstance(&instance_create_info, nullptr, &m_instance);

		if (result != VK_SUCCESS)
		{
			spdlog::error("Could not create instance.");
		}
	}

	// Check if the validation layers are supported
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

		for (const auto& property : properties)
		{
			for (const auto& layer_name : m_layers)
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
		auto err = SDL_Vulkan_CreateSurface(m_window, m_instance, static_cast<SDL_vulkanSurface*>(&m_surface));
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
			VkPhysicalDevice& phys_device = m_physical_devices.at(i);
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
				break;
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
		queue_create_info.queueFamilyIndex = family_index;

		VkPhysicalDeviceFeatures supportedFeatures{};
		vkGetPhysicalDeviceFeatures(m_chosen_phys_device.value(), &supportedFeatures);
		VkPhysicalDeviceFeatures requiredFeatures{};
		requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
		requiredFeatures.tessellationShader = VK_TRUE;
		requiredFeatures.geometryShader = VK_TRUE;
		requiredFeatures.samplerAnisotropy = VK_TRUE;

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
		buffer_info.size = RESOLUTION_X * RESOLUTION_Y * 512; // 512MB
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
		image_info.extent = { RESOLUTION_X, RESOLUTION_Y, 1 };
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

	uint32_t Renderer_Vk::choose_heap_from_flags(const VkMemoryRequirements& memoryRequirements,
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
			for (const auto& extension : device_extensions)
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
				.width = RESOLUTION_X,
				.height = RESOLUTION_Y,
			},
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_MAILBOX_KHR,

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

		Model model = tools::import_model("C:/Expectre/assets/bunny.obj");

		const std::vector<Vertex> vertices = model.vertices;
		uint32_t vertex_buffer_size =
			static_cast<uint32_t>((vertices.size() * sizeof(Vertex)));
		// Setup indices
		// std::vector<uint32_t> index_buffer{0, 1, 2, 2, 3, 0};
		std::vector<uint32_t> index_buffer = model.indices;
		m_indices.count = static_cast<uint32_t>(index_buffer.size());
		uint32_t index_buffer_size = m_indices.count * sizeof(index_buffer[0]);

		VkMemoryAllocateInfo mem_alloc{};
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements mem_reqs{};

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

		void* data;

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
		mem_alloc.memoryTypeIndex = mem_index;
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
		VkCommandBuffer copy_cmd{};

		VkCommandBufferAllocateInfo command_buffer_alloc_info{};
		command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		command_buffer_alloc_info.commandPool = m_cmd_pool;
		command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		command_buffer_alloc_info.commandBufferCount = 1;
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

		m_depth.format = VK_FORMAT_D32_SFLOAT_S8_UINT;

		// Create an optimal image used as the depth stencil attachment
		// const depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT
		VkImageCreateInfo image_info{};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = m_depth.format;
		// Use example's height and width
		image_info.extent = { RESOLUTION_X, RESOLUTION_Y, 1 };
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(m_device, &image_info, nullptr, &m_depth.image));

		// Allocate memory for the image (device local) and bind it to our image
		VkMemoryAllocateInfo mem_alloc{};
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements mem_reqs;
		vkGetImageMemoryRequirements(m_device, m_depth.image, &mem_reqs);
		mem_alloc.allocationSize = mem_reqs.size;
		bool found = tools::find_matching_memory(mem_reqs.memoryTypeBits,
			m_phys_memory_properties.memoryTypes,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex);
		assert(found);
		VK_CHECK_RESULT(vkAllocateMemory(m_device, &mem_alloc, nullptr, &m_depth.mem));
		VK_CHECK_RESULT(vkBindImageMemory(m_device, m_depth.image, m_depth.mem, 0));

		// Create a view for the depth stencil image
		// Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
		// This allows for multiple views of one image with differing ranges (e.g. for different layers)
		VkImageViewCreateInfo depth_view_info{};
		depth_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		depth_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depth_view_info.format = m_depth.format;
		depth_view_info.subresourceRange = {};
		depth_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT)
		if (m_depth.format >= VK_FORMAT_D16_UNORM_S8_UINT)
		{
			depth_view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		depth_view_info.subresourceRange.baseMipLevel = 0;
		depth_view_info.subresourceRange.levelCount = 1;
		depth_view_info.subresourceRange.baseArrayLayer = 0;
		depth_view_info.subresourceRange.layerCount = 1;
		depth_view_info.image = m_depth.image;
		VK_CHECK_RESULT(vkCreateImageView(m_device, &depth_view_info, nullptr, &m_depth.view));

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
		VkShaderModule vert_shader_module = tools::createShaderModule(m_device, "C:/Expectre/shaders/vert.vert.spv");
		VkShaderModule frag_shader_module = tools::createShaderModule(m_device, "C:/Expectre/shaders/frag.frag.spv");

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
			framebuffer_info.width = RESOLUTION_X;
			framebuffer_info.height = RESOLUTION_Y;
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
		clear_col[0] = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		clear_col[1].depthStencil = { 1.0f, 0 };
		VkRenderPassBeginInfo renderpass_info = {};
		renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpass_info.pNext = nullptr;
		renderpass_info.renderPass = m_render_pass;
		renderpass_info.framebuffer = m_framebuffers[m_current_frame];
		renderpass_info.renderArea.offset.x = 0;
		renderpass_info.renderArea.offset.y = 0;
		renderpass_info.renderArea.extent.width = RESOLUTION_X;
		renderpass_info.renderArea.extent.height = RESOLUTION_Y;
		renderpass_info.clearValueCount = 2;
		renderpass_info.pClearValues = clear_col.data();

		vkCmdBeginRenderPass(current_cmd_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(current_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

		VkViewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)RESOLUTION_X,
			.height = (float)RESOLUTION_Y,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		vkCmdSetViewport(current_cmd_buffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = RESOLUTION_X;
		scissor.extent.height = RESOLUTION_Y;

		vkCmdSetScissor(current_cmd_buffer, 0, 1, &scissor);

		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(current_cmd_buffer, 0, 1, &m_vertices.buffer, offsets);

		vkCmdBindIndexBuffer(current_cmd_buffer, m_indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(current_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_pipeline_layout, 0, 1,
			&m_uniform_buffers[m_current_frame].descriptorSet,
			0, nullptr);

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
		update_uniform_buffer();
		vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame]);
		vkResetCommandBuffer(m_cmd_buffers[m_current_frame], 0);
		record_command_buffer();

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
		VkMemoryRequirements mem_reqs;
		VkBufferCreateInfo buffer_info{};
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.allocationSize = 0;
		alloc_info.memoryTypeIndex = 0;

		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = sizeof(UBO);
		buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		for (auto i = 0; i < MAX_CONCURRENT_FRAMES; i++)
		{
			VK_CHECK_RESULT(vkCreateBuffer(m_device, &buffer_info, nullptr, &m_uniform_buffers[i].buffer));
			// Get memory requirements of uniform buffer
			vkGetBufferMemoryRequirements(m_device, m_uniform_buffers[i].buffer, &mem_reqs);
			alloc_info.allocationSize = mem_reqs.size;

			uint32_t mem_index;
			// Get memory index of host visibe + coherent
			bool found = tools::find_matching_memory(
				mem_reqs.memoryTypeBits, m_phys_memory_properties.memoryTypes,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&mem_index);
			assert(found);
			alloc_info.memoryTypeIndex = mem_index;
			// Allocate memory for uniformbuffer
			VK_CHECK_RESULT(vkAllocateMemory(m_device, &alloc_info, nullptr, &m_uniform_buffers[i].memory));
			// Bind memory to buffer
			VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_uniform_buffers[i].buffer, m_uniform_buffers[i].memory, 0));
			// We map the buffer once so we can update it without having to map it again
			VK_CHECK_RESULT(vkMapMemory(m_device,
				m_uniform_buffers[i].memory, 0,
				sizeof(UBO), 0,
				(void**)&m_uniform_buffers[i].mapped));
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
		stbi_uc* pixels = stbi_load("C:/Expectre/assets/textures/hello4.jpg",
			&tex_width, &tex_height,
			&tex_channels, STBI_rgb_alpha);
		VkDeviceSize image_size = tex_width * tex_height * 4;

		if (!pixels)
		{
			spdlog::error("Failed to load texture!");
			std::terminate();
		}

		VkBuffer staging_buffer;
		VkDeviceMemory staging_buffer_memory;
		create_buffer(image_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging_buffer, staging_buffer_memory);

		void* data;
		VK_CHECK_RESULT(vkMapMemory(m_device, staging_buffer_memory, 0, image_size, 0, &data));
		memcpy(data, pixels, static_cast<size_t>(image_size));
		vkUnmapMemory(m_device, staging_buffer_memory);

		stbi_image_free(pixels);

		create_image(tex_width, tex_height,
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_texture_image,
			m_texture_image_memory);
		transition_image_layout(m_texture_image, VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copy_buffer_to_image(staging_buffer, m_texture_image,
			static_cast<uint32_t>(tex_width),
			static_cast<uint32_t>(tex_height));
		transition_image_layout(m_texture_image, VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(m_device, staging_buffer, nullptr);
		vkFreeMemory(m_device, staging_buffer_memory, nullptr);
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
		vkGetPhysicalDeviceProperties(m_chosen_phys_device.value(), &properties);
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
			dir += glm::vec3(0.0f, 0.0f, 1.0f);
		if (m_camera.moveBack)
			dir += glm::vec3(0.0f, 0.0f, -1.0f);
		if (m_camera.moveLeft)
			dir += glm::vec3(1.0f, 0.0f, 0.0f);
		if (m_camera.moveRight)
			dir += glm::vec3(-1.0f, 0.0f, 0.0f);

		if (glm::length(dir) > 0.0f)
			dir = glm::normalize(dir);

		m_camera.pos += dir * m_camera.camera_speed * static_cast<float>(delta_t) / 1000.0f;

	}
	void Renderer_Vk::on_input_event(const SDL_Event& event)
	{

		const bool is_down = (event.type == SDL_KEYDOWN);
		switch (event.key.keysym.sym)
		{
		case SDLK_w:
			m_camera.moveForward = is_down;
			break;
		case SDLK_s:
			m_camera.moveBack = is_down;
			break;
		case SDLK_a:
			m_camera.moveLeft = is_down;
			break;
		case SDLK_d:
			m_camera.moveRight = is_down;
			break;
		}

		// do same for keyup
		const bool is_up = (event.type == SDL_KEYUP);

		if (event.type == SDL_KEYUP)
		{
			switch (event.key.keysym.sym)
			{
			case SDLK_w:
				m_camera.moveForward = is_up;
				break;
			case SDLK_s:
				m_camera.moveBack = is_up;
				break;
			case SDLK_a:
				m_camera.moveLeft = is_up;
				break;
			case SDLK_d:
				m_camera.moveRight = is_up;
				break;
			}
		}
	}

}