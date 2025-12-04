# OBS Soundboard

OBS plugin that adds a soundboard dock.

## Features

- **Sound Management**: Add, edit, and remove sounds with customizable names and hotkeys
- **Playback Controls**: Play, pause, stop, and seek through sounds with a built-in media player
- **List/Grid View**: Switch between list and grid layouts for organizing your sounds
- **Drag & Drop**: Easily add sounds by dragging audio files into the soundboard
- **Loop Support**: Enable looping for individual sounds
- **Volume Control**: Adjust volume per sound
- **Hotkeys**: Assign keyboard shortcuts to trigger individual sounds
- **System Audio Output Toggle**: Play sounds through your system audio output (speakers/headphones) in addition to OBS

### System Audio Output

The system audio output toggle allows you to selectively route soundboard audio to your physical audio devices (speakers/headphones) while still sending audio to your OBS stream/recording.

**Use Case**: This is particularly useful when using virtual audio devices (like BlackHole on macOS, VB-Cable on Windows, or PulseAudio on Linux) for selective audio routing:
- Your microphone → Virtual Audio Device → Streaming Apps (so you don't hear yourself)
- Soundboard → Both Virtual Audio Device + System Audio (so both you and your stream hear it)

**How to Use**:
1. Click the speaker/audio toggle button in the soundboard toolbar (to the right of the settings icon)
2. When **enabled** (button pressed): Sounds play through both OBS output AND your system speakers/headphones
3. When **disabled** (button not pressed): Sounds only play through OBS output (default behavior)

The toggle state is saved with your OBS scene collection and persists across restarts.

**Cross-Platform Support**: Works on Windows, macOS, and Linux using Qt6 Multimedia.
