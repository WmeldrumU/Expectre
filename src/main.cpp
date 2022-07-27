// Excpectre.cpp : Defines the entry point for the application.
//
#include <iostream>
#include "spdlog.h"

using namespace std;

int main(int argc, char **argv)
{
	cout << "Hello Expectre" << endl;

	spdlog::info("Welcome to spdlog!");

	try {
		//setup
		while (true) {
			//processInput
			//update()
			//render()
		}
	}
	catch (exception &e) {
		cout << "EXCEPTION: \n" << e.what() << endl;
		return 1;
	}
	

	return 0;
}
