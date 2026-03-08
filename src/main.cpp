#include <iostream>
#include "Engine.h"
#include "spdlog/spdlog.h"
#include "AppTime.h"

#include <crtdbg.h>

// TODO: Fix long warnings, camera postiion/movement, heap corruption
int main(int argc, char *argv[])
{

#if defined(__clang__)
		std::cout << "Compiled with clang\n";
#else
	std::cout << "Other compiler\n";
#endif
	Time::Instance().Update();
	spdlog::set_level(spdlog::level::debug);
	try
	{
		std::cout << "STARTING UP...." << std::endl;
		Expectre::Engine engine{};
		engine.run();
	}
	catch (std::exception &e)
	{
		std::cout << "EXCEPTION: \n"
				  << e.what() << std::endl;
		return 1;
	}

	return 0;
}