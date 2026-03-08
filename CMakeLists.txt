cmake_minimum_required(VERSION 3.10)

# set the project name
project(Expectre)

# use c++ 20, this version is required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
# set path for libraries/third party code
# set(LIB ${PROJECT_SOURCE_DIR}/lib)
# we are able to call this b/c the previous two lines
# find_package(SDL2 REQUIRED)
# Find Vulkan Path using CMake's Vulkan Module
# This will return Boolean 'Vulkan_FOUND' indicating the status of find as success(ON) or fail(OFF).
# Include directory path - 'Vulkan_INCLUDE_DIRS' and 'Vulkan_LIBRARY' with required libraries.
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

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