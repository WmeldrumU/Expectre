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
		// chrono::milliseconds engineClock(1000);
		VkExtent2D _windowExtent{1700, 900};

		cout << "Hello Expectre" << endl;

		spdlog::info("Welcome to spdlog!");
		SDL_Window *window = nullptr;
		SDL_Surface *screenSurface = nullptr;
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
		if (window == nullptr)
		{
			fprintf(stderr, "could not create window: %s\n", SDL_GetError());
			return 1;
		}

		SDL_Renderer *ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (ren == nullptr)
		{
			SDL_DestroyWindow(window);
			std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
			SDL_Quit();
			return 1;
		}

		std::string imagePath = "assets\\hello.bmp";
		SDL_Surface *bmp = SDL_LoadBMP(imagePath.c_str());
		if (bmp == nullptr)
		{
			SDL_DestroyRenderer(ren);
			SDL_DestroyWindow(window);
			std::cout << "SDL_LoadBMP Error: " << SDL_GetError() << std::endl;
			SDL_Quit();
			return 1;
		}

		SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, bmp);
		SDL_FreeSurface(bmp);
		if (tex == nullptr)
		{
			SDL_DestroyRenderer(ren);
			SDL_DestroyWindow(window);
			std::cout << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
			SDL_Quit();
			return 1;
		}

		for (int i = 0; i < 3; i++)
		{
			SDL_RenderClear(ren);

			SDL_RenderCopy(ren, tex, nullptr, nullptr);

			SDL_RenderPresent(ren);

			SDL_Delay(1000);
		}
		// while (true)
		// {
		// 	// processInput
		// 	// update()
		// 	// render()
		// }
		SDL_DestroyTexture(tex);
		SDL_DestroyRenderer(ren);
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
