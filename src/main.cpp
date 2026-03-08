//
#include <iostream>
#include "Volk/volk.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3/SDL_vulkan.h"
#include "spdlog/spdlog.h"
#include <crtdbg.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT        type,
	const VkDebugUtilsMessengerCallbackDataEXT* data,
	void*)                                   // pUserData
{
	std::cerr << "["
		<< ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" :
			(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN" :
			(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO" :
			"VERBOSE")
		<< "] " << data->pMessage << '\n';
	return VK_FALSE;          // do *not* abort the Vulkan call
}

void die(VkResult r, const char* what)
{
	throw std::runtime_error(std::string(what) + " (VkResult " + std::to_string(r) + ')');
}

// TODO: Fix long warnings, camera postiion/movement, heap corruption
int main(int argc, char* argv[])
{
	VkDevice device;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkInstance instance;
	SDL_Window* window = nullptr;
	VkSurfaceKHR surface;


	if (SDL_Init(SDL_INIT_VIDEO) == false)
		throw std::runtime_error(SDL_GetError());

	SDL_SetHint(SDL_HINT_RENDER_VULKAN_DEBUG, "1");


	window = SDL_CreateWindow("Volk + SDL3 + validation", 800, 600,
		SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!window) throw std::runtime_error(SDL_GetError());

	/*──── 2. Load the Vulkan loader and instance-level symbols (Volk) ─*/
	if (volkInitialize() != VK_SUCCESS)
		throw std::runtime_error("No Vulkan loader found");

	/*──── 3. Instance extensions (SDL + debug utils) ────────────────*/
	uint32_t extCount = 0;
	char const* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extCount);
	std::vector<const char*> extensions;

	for (uint32_t n = 0; n < extCount; n++)
		extensions.push_back(sdl_extensions[n]);

	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	/*──── 4. Validation layer string ────────────────────────────────*/
	const std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

	/*──── 5. Prepare debug-messenger create-info so it’s active from
			 the *moment* the instance is created ────────────────────*/
	VkDebugUtilsMessengerCreateInfoEXT dbgInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	dbgInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	dbgInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	dbgInfo.pfnUserCallback = debug_callback;

	/*──── 6. Create VkInstance ───────────────────────────────────────*/
	VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "minimal demo";
	app.apiVersion = VK_API_VERSION_1_4;

	VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	ici.ppEnabledExtensionNames = extensions.data();
	ici.enabledLayerCount = static_cast<uint32_t>(layers.size());
	ici.ppEnabledLayerNames = layers.data();
	ici.pNext = &dbgInfo;          // enable callback immediately

	if (VkResult r = vkCreateInstance(&ici, nullptr, &instance); r != VK_SUCCESS)
		die(r, "vkCreateInstance");

	volkLoadInstance(instance);                      // load vkDestroyInstance & friends


	/*──── 8. Make a surface for the window ───────────────────────────*/
	if (SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface) != true)
		throw std::runtime_error("SDL_Vulkan_CreateSurface failed");

	/*──── 9. Pick the first GPU & a queue family that can present ────*/
	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
	std::vector<VkPhysicalDevice> gpus(gpuCount);
	vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());
	VkPhysicalDevice gpu = gpus[0];

	uint32_t family = 0, famCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &famCount, nullptr);
	std::vector<VkQueueFamilyProperties> famProps(famCount);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &famCount, famProps.data());
	for (; family < famCount; ++family)
	{
		VkBool32 present = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, family, surface, &present);
		if (present && (famProps[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) break;
	}

	/*──── 10. Create logical device with swap-chain extension only ───*/
	const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qci.queueFamilyIndex = family;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;

	VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = 1;
	dci.ppEnabledExtensionNames = devExts;

	if (VkResult r = vkCreateDevice(gpu, &dci, nullptr, &device); r != VK_SUCCESS)
		die(r, "vkCreateDevice");

	volkLoadDevice(device);                          // load vkCmd* etc.

	/*──── 11. Main loop (press window close to exit) ─────────────────*/
	bool running = true;
	SDL_Event e;
	while (running)
	{
		while (SDL_PollEvent(&e))
			if (e.type == SDL_EVENT_QUIT) running = false;
		SDL_Delay(16);   // idle
	}


	vkDeviceWaitIdle(device);            // wait for all queues to finish

	// Destroy device, surface, instance
	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(instance, surface, nullptr);

	vkDestroyInstance(instance, nullptr);

	// SDL cleanup
	SDL_DestroyWindow(window);
	SDL_Quit();


	return 0;
}
