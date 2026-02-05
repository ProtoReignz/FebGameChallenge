cmake_minimum_required(VERSION 3.15)
project(MultiplayerShooter)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output binaries to build/ directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#-----------------------------------------
# Find Libraries
#-----------------------------------------

# Find Raylib
find_package(raylib QUIET)
if (NOT raylib_FOUND)
    # If not found, try pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RAYLIB REQUIRED raylib)
endif()

# Find ENet (we'll add this when you need networking)
# find_package(ENet QUIET)
# if (NOT ENet_FOUND)
#     pkg_check_modules(ENET REQUIRED libenet)
# endif()

#-----------------------------------------
# Graphics Test Executable (Your Current File)
#-----------------------------------------

add_executable(graphics_test
    graphics_test.cpp
)

target_link_libraries(graphics_test
    raylib
    m  # Math library (Linux)
)

# Platform-specific libraries
if (UNIX AND NOT APPLE)
    target_link_libraries(graphics_test pthread dl)
endif()

#-----------------------------------------
# Future: Server Executable (Uncomment when ready)
#-----------------------------------------

# add_executable(server
#     server/main.cpp
#     server/server.cpp
#     server/game_logic.cpp
# )
# 
# target_include_directories(server PRIVATE
#     ${CMAKE_SOURCE_DIR}/common
# )
# 
# target_link_libraries(server
#     enet
# )

#-----------------------------------------
# Future: Client Executable (Uncomment when ready)
#-----------------------------------------

# add_executable(client
#     client/main.cpp
#     client/client.cpp
#     client/renderer.cpp
#     client/input.cpp
# )
# 
# target_include_directories(client PRIVATE
#     ${CMAKE_SOURCE_DIR}/common
# )
# 
# target_link_libraries(client
#     raylib
#     enet
#     m
# )
# 
# if (UNIX AND NOT APPLE)
#     target_link_libraries(client pthread dl)
# endif()

#-----------------------------------------
# Build Info
#-----------------------------------------

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")