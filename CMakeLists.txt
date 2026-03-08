cmake_minimum_required(VERSION 3.15)
set(CMAKE_VERBOSE_MAKEFILE ON)
# set the project name
project(Expectre CXX)
option(USE_WEBGPU "Build with WebGPU support" OFF)


# use c++ 20, this version is required
# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(SDL3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)
find_package(assimp REQUIRED)
find_package(fmt REQUIRED)
find_package(Vulkan REQUIRED)

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
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
message(STATUS "Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")


set(COMMON_LIBRARIES
    SDL3::SDL3
    glm::glm
    spdlog::spdlog
    stb::stb
    assimp::assimp
    fmt::fmt
    Vulkan::Vulkan
)
message(STATUS "LIBRARIES: ${COMMON_LIBRARIES}")
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${COMMON_LIBRARIES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VK_NO_PROTOTYPES
)
