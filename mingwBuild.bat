rmdir build
mkdir build
conan install . --build=missing -if build -pr foo   
cmake -B build -G "Visual Studio 16 2019" -T ClangCL
cmake --build build --config Release