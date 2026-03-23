cmake_minimum_required(VERSION 3.15)
set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_policy(SET CMP0091 NEW)

# Load Conan's settings FIRST
# if(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
#     include("${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
# endif()

# set the project name
project(ExpectreProject CXX)
option(USE_WEBGPU "Build with WebGPU support" OFF)

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
find_package(xxHash REQUIRED)

# Noesis SDK root (must be set before SOURCES which references SDK paths)
if(WIN32)
    set(NOESIS_ROOT "C:/noesis")
    set(NOESIS_LIB_DIR "${NOESIS_ROOT}/Lib/windows_x86_64")
    set(NOESIS_LIB_FILE "${NOESIS_LIB_DIR}/Noesis.lib")
elseif(APPLE)
    set(NOESIS_ROOT "$ENV{HOME}/noesis")
    set(NOESIS_LIB_DIR "${NOESIS_ROOT}/Lib/macos")
    set(NOESIS_LIB_FILE "${NOESIS_LIB_DIR}/Noesis.dylib")
else()
    set(NOESIS_ROOT "/usr/local/noesis")
    set(NOESIS_LIB_DIR "${NOESIS_ROOT}/Lib/linux_x86_64")
    set(NOESIS_LIB_FILE "${NOESIS_LIB_DIR}/libNoesis.so")
endif()

    # add source files
    set(SOURCES
    src/input/InputManager.h
    src/input/InputManager.cpp
    src/Engine.cpp
    src/Engine.h
    src/ExTexture.h
    src/IRenderer.h
    src/IRendererInfo.h
    # src/IUIRenderer.h
    src/main.cpp
    src/MathUtils.h
    src/Mesh.h
    src/MeshManager.h
    src/MeshManager.cpp
    # src/NoesisShaders.h
    src/observer.h
    src/Material.h
    src/MaterialManager.h
    src/MaterialManager.cpp
    src/RenderContextVk.cpp
    src/RenderContextVk.h
    src/RenderDeviceVk.cpp
    src/RenderDeviceVk.h
    # src/RendererDx.cpp
    # src/RendererDx.h
    src/RendererVk.cpp
    src/RendererVk.h
    src/RenderResourceManager.h
    src/RenderResourceManager.cpp
    # src/RendererWgpu.cpp
    # src/RendererWgpu.h
    # src/ShaderFileWatcher.h
    src/Texture.h
    src/TextureManager.h
    src/TextureManager.cpp
    src/ToolsVk.h
    ${NOESIS_ROOT}/Src/Packages/Render/VKRenderDevice/Src/VKRenderDevice.cpp
    ${NOESIS_ROOT}/Src/Packages/App/Providers/Src/FastLZ.cpp
    src/noesis/NoesisInputAdapter.h
    src/noesis/NoesisUI.h
    src/noesis/NoesisUI.cpp
    src/AppTime.h
    src/Resource.h
    src/RenderableInfo.h
    src/ResourceManager.h
    src/ResourceHandle.h
    src/TextureVk.h
    # src/UtilsNs.h
    src/scene/CameraComponent.h
    src/scene/Camera.h
    src/scene/Camera.cpp    
    src/scene/Component.h
    src/scene/Entity.h
    src/scene/Scene.h
    src/scene/Scene.cpp
    src/scene/SceneObject.cpp
    src/scene/SceneObject.h
    src/scene/SceneRoot.h
    src/scene/MeshComponent.h
    src/scene/TransformComponent.h
    src/noesis/TextureVkNs.h
    src/noesis/RenderTargetVkNs.h
)
message(STATUS "SOURCES: ${SOURCES}")
# add the executable
add_executable(ExpectreApp ${SOURCES})


# Create a clean Noesis target
add_library(Noesis INTERFACE)
target_include_directories(Noesis INTERFACE "${NOESIS_ROOT}/Include")
target_link_libraries(Noesis INTERFACE "${NOESIS_LIB_FILE}")
set(COMMON_LIBRARIES
    SDL3::SDL3
    glm::glm
    spdlog::spdlog
    stb::stb
    assimp::assimp
    fmt::fmt
    Vulkan::Vulkan
    Vulkan::shaderc_combined
    xxHash::xxhash
    Noesis
)
message(STATUS "LIBRARIES: ${COMMON_LIBRARIES}")
target_link_libraries(ExpectreApp PUBLIC
    ${COMMON_LIBRARIES}
)
target_include_directories(ExpectreApp PRIVATE 
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/lib/
    ${NOESIS_ROOT}/Src/Packages/Render/VKRenderDevice/Src
    ${NOESIS_ROOT}/Src/Packages/Render/VKRenderDevice/Include
    ${NOESIS_ROOT}/Src/Packages/App/Providers/Src
    ${NOESIS_ROOT}/Src/Packages/App/Providers/Include
)

# We compile VKRenderDevice and FastLZ sources directly (not as a DLL),
# so disable the DLL-import macros.
target_compile_definitions(ExpectreApp PRIVATE
    NS_RENDER_VKRENDERDEVICE_API=
    NS_APP_PROVIDERS_API=
)
target_link_directories(ExpectreApp PUBLIC ${PROJECT_SOURCE_DIR}/lib/)

# Copy Noesis.dll to the build output directory so the exe can find it at runtime
if(WIN32)
    set(NOESIS_DLL "${NOESIS_ROOT}/Bin/windows_x86_64/Noesis.dll")
    add_custom_command(TARGET ExpectreApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NOESIS_DLL}"
            $<TARGET_FILE_DIR:ExpectreApp>
        COMMENT "Copying Noesis.dll to output directory"
    )
endif()

# Define it as a macro so it can be used in C++
add_compile_definitions(WORKSPACE_DIR="${CMAKE_SOURCE_DIR}")
add_compile_definitions(ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
