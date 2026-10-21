# PlayTrack

**A lightweight Windows application and game playtime tracker built with C++17 and Raylib.**

Track how much time you spend using your favorite applications and games with PlayTrack's desktop dashboard and background tracking system.

[**Download PlayTrack v1.0.0**](https://github.com/adityabhandekar77-dev/PlayTrack/releases/tag/v1.0.0)

## Features

- **Application Tracking** — Track time spent in selected Windows applications and games.
- **Automatic Process Detection** — Detect running applications and update their tracked time.
- **Background Tracking** — Run the tracker separately from the dashboard.
- **Persistent Playtime Records** — Save accumulated playtime locally between sessions.
- **Desktop Dashboard** — View your tracked applications and recorded time through a Raylib interface.
- **Application Management** — Add applications and manage which folders are scanned for executables.
- **Windows Startup Integration** — Configure tracking to start automatically when you sign in to Windows.
- **Windows Installer** — Install PlayTrack using a standalone installer without setting up the development toolchain.

## Screenshots

*Screenshots of the PlayTrack dashboard will be added here.*

## Installation

1. Visit the [PlayTrack v1.0.0 Releases page](https://github.com/adityabhandekar77-dev/PlayTrack/releases/tag/v1.0.0).
2. Download `PlayTrack-Setup-1.0.0.exe` from the release assets.
3. Run the installer and follow the setup instructions.
4. Launch PlayTrack from the Start Menu or desktop shortcut.

PlayTrack stores its local configuration and playtime data under your Windows local application data directory.

## Technology Stack

- **C++17** — Application logic
- **Raylib** — Graphical interface
- **Windows API** — Process detection and Windows integration
- **CMake** — Build configuration
- **MSYS2 UCRT64** — Development toolchain
- **nlohmann/json** — Local data persistence
- **Inno Setup** — Windows installer

## Development Setup

To build PlayTrack from source, you will need a compatible C++17 compiler, CMake, Ninja or another supported build tool, and the required Raylib and JSON dependencies.

The installer is intended for users who want to run PlayTrack without configuring the development environment.

## Project Status

**Latest release: v1.0.0**

PlayTrack's first public release is available for Windows. Testing on additional systems and further improvements will help guide future versions.

## Feedback and Contributions

Found a bug or have an idea for an improvement? Open an issue in this repository with details about the problem or suggestion.

---

Built with C++ and a love for tracking playtime.