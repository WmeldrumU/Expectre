cmake_minimum_required(VERSION 3.15)
set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_policy(SET CMP0091 NEW)

# set the project name
project(ExpectreProject CXX)
option(USE_WEBGPU "Build with WebGPU support" OFF)

# Noesis Include/Lib Paths
set(NOESIS_ROOT "C:/noesis")
include_directories(${NOESIS_ROOT}/Include)
link_directories(${NOESIS_ROOT}/Lib/windows_x86_64)

# use c++ 20, this version is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(SDL3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)
find_package(assimp REQUIRED)
find_package(fmt REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined )

set(SOURCES
    src/Engine.cpp
    src/Engine.h
    src/IRenderer.h
    src/IRendererInfo.h
    src/IUIRenderer.h
    src/main.cpp
    src/Model.h
    src/NoesisShaders.h
    src/observer.h
    src/RenderContextVk.cpp
    src/RenderContextVk.h
    src/RenderDeviceVk.cpp
    src/RenderDeviceVk.h
    src/RendererDx.cpp
    src/RendererDx.h
    src/RendererVk.cpp
    src/RendererVk.h
    src/RendererWgpu.cpp
    src/RendererWgpu.h
    src/ShaderFileWatcher.h
    src/Shared.h
    src/UIRendererVkNoesis.cpp
    src/UIRendererVkNoesis.h
    src/ToolsVk.h
    src/FastLZ.h
    src/FastLZ.cpp
    src/Time.h
    src/TextureVk.h
    src/UtilsNs.h
    src/scene/Scene.h
    src/scene/Scene.cpp
    src/scene/SceneObject.cpp
    src/scene/SceneObject.h
    src/scene/Camera.h
    src/scene/Camera.cpp
)

message(STATUS "SOURCES: ${SOURCES}")
# add the executable
add_executable(ExpectreApp ${SOURCES})

set(COMMON_LIBRARIES
    SDL3::SDL3
    glm::glm
    spdlog::spdlog
    stb::stb
    assimp::assimp
    fmt::fmt
    Vulkan::Vulkan
    Vulkan::shaderc_combined
    Noesis
)
message(STATUS "LIBRARIES: ${COMMON_LIBRARIES}")
target_link_libraries(ExpectreApp PUBLIC
    ${COMMON_LIBRARIES}
)
target_include_directories(ExpectreApp PUBLIC ${PROJECT_SOURCE_DIR}/lib/)
target_link_directories(ExpectreApp PUBLIC ${PROJECT_SOURCE_DIR}/lib/)

# Define it as a macro so it can be used in C++
add_compile_definitions(WORKSPACE_DIR="${CMAKE_SOURCE_DIR}")
