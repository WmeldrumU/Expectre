//
#include <iostream>
#ifdef USE_WEBGPU
#include <webgpu/webgpu.h>
#endif
#include "SDL2/SDL.h"
#include "Engine.h"
#include "spdlog/spdlog.h"
#include <crtdbg.h>

// TODO: Fix long warnings, camera postiion/movement, heap corruption
int main(int argc, char* argv[])
{
	spdlog::set_level(spdlog::level::debug);
	try
	{
		std::cout << "STARTING UP...." << std::endl;
		Expectre::Engine engine{};
		engine.run();
	}
	catch (std::exception& e)
	{
		std::cout << "EXCEPTION: \n"
			<< e.what() << std::endl;
		return 1;
	}

	return 0;
}
