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

	void play(const QString &path, float volume, bool loop, int64_t startTimeMs = 0);
	void stop();
	void pause();
	void resume();
	void setVolume(float volume);
	void seekTo(int64_t timeMs);
	bool isPlaying() const;
	bool isPaused() const;
	int64_t currentTime() const;

private:
	SystemAudio();
	~SystemAudio();

	struct Impl;
	Impl *impl = nullptr;
};
