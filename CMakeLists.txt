
cmake_minimum_required(VERSION 3.20)

project(PlayTrack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(PlayTrack
    src/main.cpp
    src/process_detector.cpp
    src/tracking_engine.cpp
)

target_include_directories(PlayTrack PRIVATE
    include
    "C:/Libraries/raylib/src"
    "C:/msys64/ucrt64/include"
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

add_executable(PlayTrackTracker WIN32
    src/tracker_main.cpp
    src/process_detector.cpp
    src/tracking_engine.cpp
)

target_compile_options(PlayTrackTracker PRIVATE -municode)
target_link_options(PlayTrackTracker PRIVATE -municode)

target_include_directories(PlayTrackTracker PRIVATE
    include
    "C:/msys64/ucrt64/include"
)