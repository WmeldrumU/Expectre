cmake_minimum_required(VERSION 3.15)
project(Expectre CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)
find_package(assimp REQUIRED)
find_package(Vulkan REQUIRED)

# Collect source files
file(GLOB_RECURSE SOURCES 
    ${PROJECT_SOURCE_DIR}/src/*.cpp 
    ${PROJECT_SOURCE_DIR}/lib/*.cpp 
    ${PROJECT_SOURCE_DIR}/lib/*.c 
    ${PROJECT_SOURCE_DIR}/lib/*.h 
    ${PROJECT_SOURCE_DIR}/src/*.h
)

message(STATUS "SOURCES: ${SOURCES}")

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Set common libraries
set(COMMON_LIBRARIES
    SDL2::SDL2
    SDL2::SDL2main
    glm::glm
    spdlog::spdlog
    stb::stb
    assimp::assimp
    Vulkan::Vulkan
)

message(STATUS "LIBRARIES: ${COMMON_LIBRARIES}")

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC ${COMMON_LIBRARIES})