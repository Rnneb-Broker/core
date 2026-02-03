cmake_minimum_required(VERSION 3.15)
project(highway_broker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler optimizations for Release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Suppress false-positive warning from Boost.Asio's resolver (GCC bug with memcpy bounds check)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
endif()

# Policy to use config-based package finding
cmake_policy(SET CMP0167 NEW)

# Boost - using CONFIG mode for Conan-generated packages
find_package(Boost REQUIRED COMPONENTS system CONFIG)

# ============================================================================
# Core Library (shared between broker and client)
# ============================================================================
set(CORE_SOURCES
    src/protocol/packet.cpp
    src/broker/topic_manager.cpp
    src/broker/subscription_manager.cpp
    src/data/sensor_event.cpp
)

add_library(highway_core STATIC ${CORE_SOURCES})
target_include_directories(highway_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(highway_core PUBLIC Boost::system)

# ============================================================================
# Broker Server
# ============================================================================
set(BROKER_SOURCES
    src/broker_main.cpp
    src/broker/broker.cpp
    src/broker/session.cpp
)

add_executable(broker ${BROKER_SOURCES})
target_include_directories(broker PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(broker PRIVATE highway_core Boost::system pthread)

# ============================================================================
# Client Library
# ============================================================================
set(CLIENT_SOURCES
    src/client/client.cpp
)

add_library(highway_client STATIC ${CLIENT_SOURCES})
target_include_directories(highway_client PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(highway_client PUBLIC highway_core Boost::system)

# ============================================================================
# Examples
# ============================================================================
# Sensor Simulator
add_executable(sensor_simulator examples/sensor_simulator.cpp)
target_include_directories(sensor_simulator PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(sensor_simulator PRIVATE highway_client pthread)

# Traffic Monitor
add_executable(traffic_monitor examples/traffic_monitor.cpp)
target_include_directories(traffic_monitor PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(traffic_monitor PRIVATE highway_client pthread)

# ============================================================================
# Legacy P2P Relay (keep for reference)
# ============================================================================
set(LEGACY_SOURCES
    src/main.cpp
    src/core/node.cpp
    src/core/peer_connection.cpp
    src/core/peer_manager.cpp
    src/core/router.cpp
    src/helpers/connect_server_client_arch.cpp
    src/helpers/fill_nodes_vetor.cpp
)

# Check if ALL legacy files exist
set(LEGACY_EXISTS TRUE)
foreach(src ${LEGACY_SOURCES})
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${src}")
        set(LEGACY_EXISTS FALSE)
        message(STATUS "Legacy file not found: ${src}")
        break()
    endif()
endforeach()

if(LEGACY_EXISTS)
    add_executable(relay ${LEGACY_SOURCES})
    target_include_directories(relay PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(relay PRIVATE Boost::system pthread)
else()
    message(STATUS "Skipping legacy relay build (missing source files)")
endif()

# ============================================================================
# Install
# ============================================================================
install(TARGETS broker RUNTIME DESTINATION bin)
install(TARGETS highway_core highway_client ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
