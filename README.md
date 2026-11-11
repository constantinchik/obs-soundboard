# OBS Soundboard

OBS Studio plugin that adds a soundboard dock, so you can trigger sounds while streaming or recording.

## Download

Get the latest version from the **[Releases page](https://github.com/constantinchik/obs-soundboard/releases/latest)**.

Direct links for **v0.1**:

| Platform | Download |
| --- | --- |
| Windows (installer) | [obs-soundboard-0.1.0-windows-x64-Installer.exe](https://github.com/constantinchik/obs-soundboard/releases/download/0.1.0/obs-soundboard-0.1.0-windows-x64-Installer.exe) |
| Windows (zip, extract into your OBS folder) | [obs-soundboard-0.1.0-windows-x64.zip](https://github.com/constantinchik/obs-soundboard/releases/download/0.1.0/obs-soundboard-0.1.0-windows-x64.zip) |
| Windows (portable OBS) | [obs-soundboard-0.1.0-windows-x64-Portable.zip](https://github.com/constantinchik/obs-soundboard/releases/download/0.1.0/obs-soundboard-0.1.0-windows-x64-Portable.zip) |
| macOS (Apple Silicon and Intel) | [obs-soundboard-0.1.0-macos-universal.pkg](https://github.com/constantinchik/obs-soundboard/releases/download/0.1.0/obs-soundboard-0.1.0-macos-universal.pkg) |
| Ubuntu 24.04 (x86_64) | [obs-soundboard-0.1.0-x86_64-linux-gnu.deb](https://github.com/constantinchik/obs-soundboard/releases/download/0.1.0/obs-soundboard-0.1.0-x86_64-linux-gnu.deb) |

The macOS package isn't signed by Apple: if macOS blocks it, right-click the `.pkg` and choose **Open**.

Restart OBS after installing. If the dock isn't visible, enable it from **Docks → Soundboard**.

## Features

- **Sound tiles**: each sound is a tile with a large icon (start the name with an emoji, e.g. `🚀 Lets go`) and its name underneath. Switch to a plain list from the right-click menu.
- **Click and long-press actions**: by default a click plays the sound (again, on top of what's already playing) and a long press stops it. Choose *Play again*, *Restart* or *Stop* as the soundboard-wide default from the right-click menu, and override it per sound in **Edit Sound**. Tiles with their own setting show a small dot.
- **Stacked playback**: clicking a sound that's already playing layers another copy over it. Each copy draws its own progress line across the tile, and the bottom bar follows the most recent one.
- **Hear it yourself**: the 🎧 button also plays sounds through your system's default output, so you can hear them locally while streaming or recording. Volume follows the OBS mixer slider.
- **Bottom bar**: a Stop/Play button, seek bar and timer. Hide it from the right-click menu (**Show bottom bar**) if you only want the tiles.
- **Per-sound settings**: name, audio file, volume and looping.
- **Hotkeys**: give each sound its own hotkey (it supports long press too), plus global Play, Pause, Stop, Restart, Mute and Unmute hotkeys.
- **Filters**: open the soundboard source's filters straight from the dock.
- **Saved with your scene collection**: sounds, hotkeys and dock layout persist between sessions.

## Usage

1. Open the **Soundboard** dock.
2. Click **+** (or drop audio files onto the dock) to add sounds.
3. Click a tile to play it. Hold it to stop it.
4. Right-click a tile to edit, rename, duplicate or remove it, or to change the default click and long-press actions.

### Hotkeys

Hotkeys are set in **Settings → Hotkeys**. Each sound appears as `Soundboard: '<sound name>'`, alongside the global `Soundboard: Play / Pause / Stop / Restart / Mute / Unmute` hotkeys.

## Building from source

The project uses the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate) build system and CMake presets. OBS sources, prebuilt dependencies and Qt6 are downloaded automatically according to [`buildspec.json`](buildspec.json).

```bash
# Windows (Visual Studio 2022)
cmake --preset windows-x64
cmake --build --preset windows-x64

# macOS (Xcode)
cmake --preset macos
cmake --build --preset macos

# Ubuntu
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
```

## Documentation

Extra documentation lives in [`docs/`](docs/). Images used in this README go in [`docs/assets/`](docs/assets/).

## License

Licensed under the [GNU GPL v2](LICENSE).
