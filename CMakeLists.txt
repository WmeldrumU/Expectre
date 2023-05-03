cmake_minimum_required(VERSION 3.15)
# set the project name
project(Expectre CXX)
# use c++ 20, this version is required
# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

# message(STATUS "SDL LIBRARIES: ${SDL2_LIBRARIES}")
# message(STATUS "SDL INCLUDES: ${SDL2_INCLUDE_DIRS}")
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