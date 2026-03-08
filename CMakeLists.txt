cmake_minimum_required(VERSION 3.10)

# set the project name
project(Expectre CXX)

# Include Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# use c++ 20, this version is required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})


# message(STATUS SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS})

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# add the executable
add_executable(Expectre ${SOURCES})

message(STATUS "SDL2 libs: ${SDL2_LIBRARIES}")
## LINKING ###
#SDL2
target_link_libraries(Expectre
${CONAN_LIBS}
${Vulkan_LIBRARY})