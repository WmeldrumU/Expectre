
#include <glm/glm.hpp>
#include <iostream>

#include "rendererVk.h"
#include "shared.h"
#include "spdlog/spdlog.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

struct {
        glm::mat4 modelMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
    } uboVS;


namespace expectre {
    Renderer_Vk::Renderer_Vk() {


        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO ) != 0) {
			SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
			throw std::runtime_error("failed to initialize SDL!");
		}

		if (SDL_Vulkan_LoadLibrary(NULL)) {
			SDL_Log("Unable to initialize vulkan lib: %s", SDL_GetError());
			throw std::runtime_error("SDL failed to load Vulkan Library!");
		}
		SDL_Window* gWindow = SDL_CreateWindow("WINDOW TITLE",
												SDL_WINDOWPOS_CENTERED,
												SDL_WINDOWPOS_CENTERED,
												1280, 720,
												SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
		
		if (!gWindow) {
			SDL_Log("Unable to initialize application window!: %s", SDL_GetError());
			throw std::runtime_error("Unable to initialize application window!");
		}

        create_instance();

        //createSurface();

        //cnumeratePhysicalDevices();

        //selectPhysicalDevice();

        //createLogicalDeviceAndQueues();

        //createSemaphores();

        //createCommandPool();

        //allocator.init();

        //stagingManager.Init();

        //CreateSwapChain();

        //CreateRenderTargets();

        //createRenderPass();

        //createPipelineCache();

        //createFrameBuffers();

        //renderProgManager.Init();

        //vertexCache.Init(... );


    }

    Renderer_Vk::~Renderer_Vk() {

    }

    void Renderer_Vk::create_instance() {

        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Expectre App";
        app_info.applicationVersion = 1;
        app_info.pEngineName = "Expectre";
        app_info.engineVersion = 1;
        app_info.apiVersion = VK_API_VERSION_1_3; // specify what version of vulkan we want

        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info; // create_info depends on app_info
        uint32_t num_extensions = 0;

		if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &num_extensions, nullptr)) {
			SDL_Log("Unable to get number of extensions: %s", SDL_GetError());
			throw std::runtime_error("Unable to get number of extensions");
		}
		std::vector<const char*> extensions(num_extensions);
		if (!SDL_Vulkan_GetInstanceExtensions(nullptr, &num_extensions, extensions.data())) {
            SDL_Log("Unable to get vulkan extensions: %s", SDL_GetError());
			throw std::runtime_error("Unable to get vulkan extension names:");
        }

        create_info.enabledExtensionCount = num_extensions;
        create_info.ppEnabledExtensionNames = extensions.data();

        create_info.enabledLayerCount = 0;

		std::cout << "available extensions:" << '\n';

		for (const auto& extension : extensions) {
			std::cout << '\t' << extension << '\n';
		}


        if (vkCreateInstance(&create_info, nullptr, &(this->m_instance)) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }
}