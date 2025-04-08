cmake_minimum_required(VERSION 3.15)
set(CMAKE_VERBOSE_MAKEFILE ON)
# set the project name
project(Expectre CXX)
option(USE_WEBGPU "Build with WebGPU support" OFF)

set(COMMON_LIBRARIES
    SDL2::SDL2
    SDL2::SDL2main
    glm::glm
    spdlog::spdlog
    stb::stb
    assimp::assimp
    fmt::fmt
)

# use c++ 20, this version is required
# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)
find_package(assimp REQUIRED)
find_package(fmt REQUIRED)

# message(STATUS "SDL LIBRARIES: ${SDL2_LIBRARIES}")
# message(STATUS "SDL INCLUDES: ${SDL2_INCLUDE_DIRS}")
file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/lib/*.cpp 
${PROJECT_SOURCE_DIR}/lib/*.c 
${PROJECT_SOURCE_DIR}/lib/*.h 
${PROJECT_SOURCE_DIR}/src/*.h)

message(STATUS "SOURCES: ${SOURCES}")
# add the executable
add_executable(${PROJECT_NAME} ${SOURCES})
# include is needed when using Ninja/MinGW
# target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})

if(USE_WEBGPU)
    include(FetchContent)
    FetchContent_Declare(
        webgpu
        GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
        GIT_TAG dawn
    )
    FetchContent_MakeAvailable(webgpu)
    message(STATUS "USING WEBGPU")
    # If not using WebGPU, find Vulkan
    find_package(Vulkan REQUIRED)
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} Vulkan::Vulkan)
    # If WebGPU provides a CMake target, link it, otherwise adjust as necessary
    target_link_libraries(${PROJECT_NAME} PUBLIC webgpu)
else()
    # If not using WebGPU, find Vulkan
    find_package(Vulkan REQUIRED)
    message(STATUS "USING VULKAN")
    set(COMMON_LIBRARIES ${COMMON_LIBRARIES} Vulkan::Vulkan)
endif()
message(STATUS "LIBRARIES: ${COMMON_LIBRARIES}")
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${COMMON_LIBRARIES}
)
