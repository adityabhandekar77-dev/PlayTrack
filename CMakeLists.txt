
cmake_minimum_required(VERSION 3.20)

project(PlayTrack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(PlayTrack
    src/main.cpp
)

target_include_directories(PlayTrack PRIVATE
    "C:/Libraries/raylib/src"
)

target_link_directories(PlayTrack PRIVATE
    "C:/Libraries/raylib/build/raylib"
)

target_link_libraries(PlayTrack PRIVATE
    raylib
    opengl32
    gdi32
    winmm
)
