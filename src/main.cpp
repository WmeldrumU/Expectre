//
#define NDEBUG

#include <iostream>
#include <webgpu/webgpu.h>
#include "SDL2/SDL.h"
#include "Engine.h"
#include "spdlog/spdlog.h"


int main(int argc, char *argv[])
{
	spdlog::set_level(spdlog::level::debug);
	try
	{
		std::cout << "STARTING UP...." << std::endl;
		Expectre::Engine engine{};
		engine.run();
		// engine.cleanup();
	}
	catch (std::exception &e)
	{
		std::cout << "EXCEPTION: \n"
				  << e.what() << std::endl;
		return 1;
	}

	return 0;
}
