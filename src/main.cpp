// Excpectre.cpp : Defines the entry point for the application.
//
#include <iostream>
#include <vulkan/vulkan.h>
//#include <chrono.h>

#include "spdlog/spdlog.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

using namespace std;

static bool userExit = true;

int main(int argc, char *argv[])
{
	try
	{
		// setup
		// create renderer, pass to "engine"
		
		// start engine -- engine.run()



		//create input

		//engine.cleanup()
	}
	catch (exception &e)
	{
		cout << "EXCEPTION: \n"
			 << e.what() << endl;
		return 1;
	}

	return 0;
}
