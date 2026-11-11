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

	// Extra, independent playbacks that can overlap the main one and each other.
	// playVoice returns an id (or -1 on failure); finished voices report inactive.
	int playVoice(const QString &path, float volume, bool loop);
	void stopVoice(int id);
	void stopAllVoices();
	void setVoiceVolume(int id, float volume);
	bool isVoiceActive(int id) const;

private:
	SystemAudio();
	~SystemAudio();

	struct Impl;
	Impl *impl = nullptr;
};
