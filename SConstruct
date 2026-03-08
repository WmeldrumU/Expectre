# Library('foo', ['f1.c', 'f2.c', 'f3.c'])
# cpp17 = Environment( CCFLAGS=['-std=c++17', '-Wall'], LIBPATH='./lib/SDL/lib/', LIBS=['mingw32', 'SDL2', 'SDL2main'], CPPPATH=[Glob('./lib/**/include')])
cpp17 = Environment(CCFLAGS=['-std=c++17', '-Wall'])
cpp17.Program('src/main.cpp')
