cmake_minimum_required(VERSION 3.15)
# set the project name
project(Expectre CXX)
# use c++ 20, this version is required
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# add the executable
add_executable(${PROJECT_NAME} ${SOURCES})
# include is needed when using Ninja/MinGW
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
target_link_libraries(Expectre 
    ${SDL2_LIBRARIES}
    SDL2::SDL2
    SDL2::SDL2main
    Vulkan::Vulkan
)