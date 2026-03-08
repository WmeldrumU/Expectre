# Expectre

conan install . --build=missing -if build -pr foo   
cmake -B build -G "Visual Studio 16 2019" -T ClangCL
cmake --build build --config Release

PS C:\Expectre> conan profile show foo  
Configuration for profile foo:

[settings]
os=Windows
os_build=Windows
arch=x86_64
arch_build=x86_64
compiler=Visual Studio
compiler.toolset=ClangCL
compiler.version=16
build_type=Release
[options]
[conf]
[build_requires]
[env]







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

[settings]
arch=x86_64
build_type=Debug
compiler=clang
compiler.cppstd=gnu14
compiler.libcxx=libstdc++
compiler.version=10
os=Linux
[conf]
tools.system.package_manager:mode=install
tools.system.package_manager:sudo = True

