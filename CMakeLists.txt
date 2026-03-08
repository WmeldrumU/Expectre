cmake_minimum_required(VERSION 3.15)
# set the project name
project(Expectre CXX)
# use c++ 20, this version is required
# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
# 1. Look for a SDL2 package, 2. look for the SDL2 component and 3. fail if none can be found
find_package(SDL2 REQUIRED)
# 1. Look for a SDL2 package, 2. Look for the SDL2maincomponent and 3. DO NOT fail when SDL2main is not available
# find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
find_package(Vulkan REQUIRED)

message(STATUS "SDL LIBRARIES: ${SDL2_LIBRARIES}")
message(STATUS "SDL INCLUDES: ${SDL2_INCLUDE_DIRS}")
file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# add the executable
add_executable(${PROJECT_NAME} ${SOURCES})
# include is needed when using Ninja/MinGW
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC
    SDL2::SDL2
    SDL2::SDL2main
    Vulkan::Vulkan
)