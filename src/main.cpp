// Excpectre.cpp : Defines the entry point for the application.
//
#include <iostream>
#include "SDL.h"
#include "rendererVk.h"

int main(int argc, char* argv[])
{
	try
	{
		std::cout << "STARTING UP...." << std::endl;
		vkCreateInstance(NULL, NULL, NULL);
		// setup window
		expectre::Renderer_Vk renderer{};

		// create renderer, pass to "engine"
		// start engine -- engine.run()



		//create input

		//engine.cleanup()
	}
	catch (std::exception &e)
	{
		std::cout << "EXCEPTION: \n"
			 << e.what() << std::endl;
		return 1;
	}

	return 0;
}
