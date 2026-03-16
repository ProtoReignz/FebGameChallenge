cmake_minimum_required(VERSION 3.15)
project(MineNMayhem CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # hard error if compiler can't do C++20

include(FetchContent)

# ── Raylib ──────────────────────────────────────────────────────
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ── ENet ────────────────────────────────────────────────────────
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(enet)

# ── Shared headers ──────────────────────────────────────────────
# game.h lives next to this file, so the source dir is enough.
include_directories(${CMAKE_SOURCE_DIR})

# ── Server ──────────────────────────────────────────────────────
add_executable(server server.cpp)
target_link_libraries(server PRIVATE raylib enet)

# ── Client ──────────────────────────────────────────────────────
add_executable(client client.cpp)
target_link_libraries(client PRIVATE raylib enet)

# ── Windows extras ──────────────────────────────────────────────
if(WIN32)
    foreach(tgt server client)
        target_link_libraries(${tgt} PRIVATE winmm ws2_32)
    endforeach()
endif()