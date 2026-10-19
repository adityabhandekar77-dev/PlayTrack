PlayTrack font consistency + live scan-folder refresh

Copy these files to C:\Code\PlayTrack, preserving folders:
- src/main.cpp
- src/folder_picker.cpp
- src/tracker_main.cpp
- src/app_config.cpp
- include/folder_picker.h
- include/app_config.h
- CMakeLists.txt

What changed:
- Dashboard and folder-manager buttons now use the same drawText/font path.
- Background tracker reloads config.json at each scan interval and rebuilds its scan-folder list.
- Newly discovered executables are merged into playtime.json and then picked up by the dashboard refresh.
- Corrected config JSON newline output and removed redundant NOMINMAX define.

Before copying, back up the listed source files. Do not replace config.json or playtime.json.
Build with:
  cd /c/Code/PlayTrack
  cmake -S . -B build
  cmake --build build

Note: the source has not been compiled in the user's Windows environment; verify the build before running.
