# PlayTrack first-run startup integration (draft)

This package adds automatic tracker launch and Windows startup registration. It is not yet a verified release installer.

## Intended behavior
- The user installs PlayTrack with the Inno Setup installer and gets one desktop shortcut (optional in the installer task; Start Menu shortcut is always created).
- Opening PlayTrack launches `PlayTrackTracker.exe` if it is not already running.
- When the dashboard successfully starts the tracker, it creates/updates `PlayTrack Tracker.lnk` in the current Windows user's Startup folder.
- The tracker runs hidden and can continue after the dashboard closes. `S` remains the dashboard start/stop toggle. Stopping it manually does not remove the Startup shortcut, so it starts again at the next sign-in.

## Apply source changes
1. Back up `src/main.cpp` and `CMakeLists.txt`.
2. Replace `src/main.cpp` with `PlayTrack-main-auto-start.cpp` (rename to `main.cpp`).
3. Copy `startup_manager.cpp` to `src/startup_manager.cpp`, and `startup_manager.h` to `include/startup_manager.h`.
4. Add `src/startup_manager.cpp` to the `PlayTrack` target only in `CMakeLists.txt`. The supplied `CMakeLists-PlayTrack-startup.txt` shows the intended addition; merge it into your actual current file rather than overwriting unrelated changes.
5. Build and test before committing.

## Installer template
`PlayTrack-Setup.iss` is an Inno Setup template. It expects `release\PlayTrack.exe`, `release\PlayTrackTracker.exe`, and any runtime DLLs required by the build. Install Inno Setup to compile it. Keep personal `config.json`, `playtime.json`, backups, and `.lnk` files out of `release`.

## Required tests before calling this 1.0.0
- Build with your current MSYS2/Raylib toolchain.
- Verify launch from installed desktop shortcut, including a path with spaces.
- Verify tracker starts once, not twice, and dashboard S toggles correctly.
- Sign out/in or reboot and verify tracker starts without showing a console window.
- Verify uninstall removes the installed app and shortcuts but does not delete user playtime/config data.
- Test paths with non-ASCII characters if those are intended to be supported.

This draft has not been compiled or run on the user's Windows machine. The installer is a starting template, not a validated release build.
