make:
	g++ -o ./src/main.exe .\src\main.cpp -std=c++17 -Wall -Ilib/SDL/include/ -Llib/SDl/lib -lmingw32 -lSDL2main -lSDL2 