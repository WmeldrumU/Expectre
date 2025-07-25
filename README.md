# Expectre
C++20/Vulkan/SDL2 based renderer


## Build

Using Conan 2 + CMake + Clang

Current build for Windows:
#### Conan profile (default/host)
```
[settings]
arch=x86_64
build_type=Debug
compiler=msvc
compiler.cppstd=20
compiler.runtime=dynamic
compiler.version=193
os=Windows

```

#### Conan profile (clang-cl)
```
[settings]
os=Windows
arch=x86_64
build_type=Debug
compiler=clang
compiler.version=19
compiler.cppstd=gnu20
compiler.runtime=dynamic
compiler.runtime_type=Debug
compiler.runtime_version=v143

[conf]
tools.cmake.cmaketoolchain:generator=Visual Studio 17
```

Build commands
```
conan install . -pr:h clang-cl -pr:b clang-cl --build=missing
cmake -B build -DCMAKE_TOOLCHAIN_FILE="Debug/generators/conan_toolchain.cmake" 
cmake --build build --config Debug
```

TODOS
- runtime shader compilation
- noesis integration
- zoom to fit
- Create separate VkQueue for layout transitions so graphic's queue isn't blocked by resource loading
- Add cmake logic to copy Noesis.dll to build directory
- Move end_single_time_commands out of rendererVk to some utility/tools class