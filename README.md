# Expectre
C++20/Vulkan/SDL2 based renderer


## Build

Using Conan 2 + CMake + Clang

Current build for Windows:

#### Conan profile
```
[settings]
arch=x86_64
build_type=Debug
compiler=clang
compiler.cppstd=gnu23
compiler.runtime=dynamic
compiler.runtime_type=Debug
compiler.runtime_version=v143
compiler.version=14
os=Windows
[conf]
tools.cmake.cmaketoolchain:generator=Visual Studio 17
```

Build commands
```
conan install . --build=missing -pr clang-cl
cmake -B build -DCMAKE_TOOLCHAIN_FILE="generators/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

