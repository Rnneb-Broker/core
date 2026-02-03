cmake_minimum_required(VERSION 3.15)
project(relay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost
find_package(Boost REQUIRED COMPONENTS system)

# SDL2
# find_package(SDL2 REQUIRED)

# Sources
file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(relay ${SOURCES})

target_include_directories(relay
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(relay
    PRIVATE
        Boost::system
        # SDL2::SDL2
)
