#pragma once

#include <QString>

// Platform-native audio playback for system audio output
// Does not require Qt Multimedia - uses native APIs:
// - macOS: NSSound (AppKit)
// - Windows: MCI (Media Control Interface)
// - Linux: paplay/aplay subprocess

class SystemAudio {
public:
	static SystemAudio *instance();

	void play(const QString &path, float volume, bool loop);
	void stop();
	void setVolume(float volume);
	bool isPlaying() const;

private:
	SystemAudio();
	~SystemAudio();

	struct Impl;
	Impl *impl = nullptr;
};
