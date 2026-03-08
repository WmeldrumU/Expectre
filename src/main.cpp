// Excpectre.cpp : Defines the entry point for the application.
//
#include <iostream>
#include "SDL2/SDL.h"
#include "Engine.h"
#include "rendererVk.h"

int main(int argc, char *argv[])
{
	try
	{
		std::cout << "STARTING UP...." << std::endl;
		Expectre::Renderer_Vk renderer{};
		Expectre::Engine engine{&renderer};
		// setup window

		// create renderer, pass to "engine"
		// start engine -- engine.run()

		// create input

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
