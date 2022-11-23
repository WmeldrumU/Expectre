﻿// Excpectre.cpp : Defines the entry point for the application.
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
		// chrono::milliseconds engineClock(1000);
		VkExtent2D _windowExtent{1700, 900};

		cout << "Hello Expectre" << endl;

		spdlog::info("Welcome to spdlog!");
		SDL_Window *window = NULL;
		SDL_Surface *screenSurface = NULL;
		if (SDL_Init(SDL_INIT_VIDEO) < 0)
		{
			fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
			return 1;
		}
		window = SDL_CreateWindow(
			"hello_sdl2",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			SCREEN_WIDTH, SCREEN_HEIGHT,
			SDL_WINDOW_SHOWN);
		if (window == NULL)
		{
			fprintf(stderr, "could not create window: %s\n", SDL_GetError());
			return 1;
		}
		screenSurface = SDL_GetWindowSurface(window);
		SDL_Rect rect1;
		rect1.h = 200;
		rect1.w = 200;
		rect1.x = 50;
		rect1.y = 100;

		SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0xFF, 0xFF, 0xFF));
		SDL_FillRect(screenSurface, &rect1, SDL_MapRGB(screenSurface->format, 0x00, 0x00, 0x00));
		SDL_UpdateWindowSurface(window);

		while (true)
		{
			// processInput
			// update()
			// render()
		}
		SDL_DestroyWindow(window);
		SDL_Quit();
	}
	catch (exception &e)
	{
		cout << "EXCEPTION: \n"
			 << e.what() << endl;
		return 1;
	}

	return 0;
}
