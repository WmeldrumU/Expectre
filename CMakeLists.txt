cmake_minimum_required(VERSION 3.10)

# set the project name
project(Expectre)

# use c++ 17, this version is required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE ON)
# set path for libraries/third party code
set(LIB ${PROJECT_SOURCE_DIR}/lib)
# set path for cmake specific code, currently used for FindSDL2.cmake
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
# path for SDL2, required for FindSDL2.cmake
set(SDL2_DIR  "${CMAKE_SOURCE_DIR}/lib/SDL")
set(VULKAN_DIR "C:/VulkanSDK/1.3.224.1/Include")
# we are able to call this b/c the previous two lines
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${VULKAN_DIR})

# message(STATUS SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS})

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# add the executable
add_executable(Expectre ${SOURCES})


file(GLOB LIB_SUBDIRS LIST_DIRECTORIES true ${LIB}/*)
message(STATUS "LIB_SUBIDRS ${LIB_SUBDIRS}")

# include lib/*/include
foreach(SUBDIR ${LIB_SUBDIRS}) # for each item of lib/*
    message(STATUS "SUBDIR: ${SUBDIR}")
    IF(EXISTS ${SUBDIR}/include) # if item is a directory, include it
        message(STATUS "Including... ${SUBDIR}")
        target_include_directories(${PROJECT_NAME} PUBLIC ${SUBDIR}/include)
    endif()
endforeach()

message(STATUS "SDL2 libs: ${SDL2_LIBRARIES}")


## LINKING ###
#SDL2
target_link_libraries(Expectre ${SDL2_LIBRARIES}) 