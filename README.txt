PlayTrack clean background-tracker dashboard integration

1. Keep a backup of your current src/main.cpp.
2. Replace src/main.cpp with the included src/main.cpp.
3. Keep your current CMakeLists.txt; it already builds PlayTrackTracker with -municode.
4. Build from MSYS2 UCRT64 in /c/Code/PlayTrack:
   cmake -S . -B build
   cmake --build build
5. Make sure build/PlayTrack.exe and build/PlayTrackTracker.exe are in the same folder.
6. Run build/PlayTrack.exe. Press S to start/stop the background tracker.

Behavior:
- Closing the dashboard does not stop the tracker.
- Dashboard polls saved totals and process status while the tracker is active.
- To avoid concurrent edits to playtime.json, add applications only while tracking is stopped.
- This source has not been compiled on your machine; build and test before committing.
