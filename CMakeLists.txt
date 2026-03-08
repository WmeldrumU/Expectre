cmake_minimum_required(VERSION 3.10)

# set the project name
project(Expectre)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(LIB ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(SDL2_DIR  "${CMAKE_SOURCE_DIR}/lib/SDL")
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

message(STATUS SDL2_INCLUDE_DIRS: ${SDL2_INCLUDE_DIRS})

# message(STATUS ${LIB})
# find_path(SDL2_INCLUDE_DIR NAME SDL.h HINTS SDL2)
# message(STATUS ${PROJECT_SOURCE_DIR})
# find_library(SDL2_LIBRARY NAME SDL2)
# file(GLOB_RECURSE INCLUDE_DIRS LIST_DIRECTORIES true
#     RELATIVE ${CMAKE_SOURCE_DIR} "lib/*/include")



file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# add the executable
add_executable(Expectre ${SOURCES})


file(GLOB LIB_SUBDIRS LIST_DIRECTORIES true ${LIB}/*)

#include lib/*/include
message(STATUS "LIB_SUBIDRS ${LIB_SUBDIRS}")
foreach(SUBDIR ${LIB_SUBDIRS})
    message(STATUS "SUBDIR: ${SUBDIR}")
    IF(EXISTS ${SUBDIR}/include)
        message(STATUS "Including... ${SUBDIR}")
        target_include_directories(${PROJECT_NAME} PUBLIC ${SUBDIR}/include)
    endif()
endforeach()

message(STATUS "SDL2 libs: ${SDL2_LIBRARIES}")

#SDL2 Linking
target_link_libraries(Expectre ${SDL2_LIBRARIES}) 