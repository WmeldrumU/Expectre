// Excpectre.cpp : Defines the entry point for the application.
//
#define NDEBUG

#include <iostream>
#include "SDL2/SDL.h"
#include "Engine.h"
#include "rendererVk.h"
#include "spdlog/spdlog.h"



int main(int argc, char *argv[])
{
	spdlog::set_level(spdlog::level::debug);
	try
	{
		std::cout << "STARTING UP...." << std::endl;
		Expectre::Engine engine{};
		engine.run();
		engine.cleanup();
	}
	catch (std::exception &e)
	{
		std::cout << "EXCEPTION: \n"
				  << e.what() << std::endl;
		return 1;
	}

	return 0;
}
