# OBS Soundboard

OBS plugin that adds a soundboard dock.

## Features

- Add, edit, and remove sounds with drag-and-drop support
- Hotkey support for triggering sounds
- Per-sound volume and loop settings
- Grid or list view modes
- **System Audio Output**: Play sounds through your system's default audio output in addition to the OBS virtual audio track. This allows you to hear the sounds locally while streaming/recording. Features include:
  - Supports MP3, OGG, WAV, FLAC, and other common audio formats on all platforms
  - Volume syncs with the OBS mixer slider in real-time
  - Media controls (play, pause, seek) are synchronized
  - Enable monitoring mid-playback and it starts from the current position

## Installation

### Windows

**Option 1: Installer (Recommended)**
1. Download `obs-soundboard-*-windows-x64-Installer.exe` from the [Releases](https://github.com/cg2121/obs-soundboard/releases) page or CI artifacts
2. Run the installer and follow the prompts
3. Restart OBS Studio

**Option 2: Portable/Manual Installation**
1. Download `obs-soundboard-*-windows-x64-Portable.zip` from the [Releases](https://github.com/cg2121/obs-soundboard/releases) page or CI artifacts
2. Extract the zip file
3. Copy the contents to your OBS installation directory:
   - Copy `obs-plugins/64bit/obs-soundboard.dll` to `C:\Program Files\obs-studio\obs-plugins\64bit\`
   - Copy `data/obs-plugins/obs-soundboard/` folder to `C:\Program Files\obs-studio\data\obs-plugins\`
4. Restart OBS Studio

### macOS

1. Download `obs-soundboard-*-macos-universal.pkg` from the [Releases](https://github.com/cg2121/obs-soundboard/releases) page
2. Run the installer package
3. Restart OBS Studio

### Linux (Ubuntu)

1. Download `obs-soundboard-*-x86_64.deb` from the [Releases](https://github.com/cg2121/obs-soundboard/releases) page
2. Install with: `sudo dpkg -i obs-soundboard-*-x86_64.deb`
3. Restart OBS Studio

## Usage

1. Open OBS Studio
2. Go to **View** > **Docks** > **Soundboard** to show the soundboard dock
3. Click the **+** button to add sounds
4. Configure hotkeys in **Settings** > **Hotkeys**

## Building from Source

This project uses CMake and requires:
- CMake 3.28+
- Qt6
- OBS Studio 31.0.0+ development files

### Windows
Requires Visual Studio 2022:
```bash
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
cmake --install build_x64 --prefix release --config RelWithDebInfo
```

### macOS
```bash
cmake --preset macos
cmake --build --preset macos --config RelWithDebInfo
```

### Linux
```bash
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64 --config RelWithDebInfo
```
