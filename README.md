# Expectre






clang-cl profile
[settings]
os=Windows
arch=x86_64
build_type=Debug
compiler=clang
compiler.version=14
compiler.cppstd=gnu14
compiler.runtime=dynamic
compiler.runtime_type=Debug
compiler.runtime_version=v142
 
[conf]
#tools.env.virtualenv:auto_use=True
tools.cmake.cmaketoolchain:generator=Visual Studio 16

conan2 install . --output-folder=build --build=missing -pr clang-cl 
cd ./build/
cmake ../ -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_BUILD_TYPE="Debug"
cmake --build . --config Debug

UBUNTU
export CC=clang CXX=clang++


See https://blog.conan.io/2022/10/13/Different-flavors-Clang-compiler-Windows.html for info on profiles/compiler

Make sure vulkan + optional glm headers are installed, as well as the vulkan environemtn variables (these should be set by the installer on windows)
conan2 install . -pr clang-cl --build=missing        

cmake -B build -DCMAKE_TOOLCHAIN_FILE="build/generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE="Debug" --debug-find                         

cmake --build .\build\ --config debug     