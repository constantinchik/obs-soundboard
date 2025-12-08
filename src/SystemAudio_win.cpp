#ifdef _WIN32

#define MINIAUDIO_IMPLEMENTATION
#include "../deps/miniaudio/miniaudio.h"

#include "SystemAudio.hpp"

struct SystemAudio::Impl {
	ma_engine engine;
	ma_sound sound;
	bool engineInitialized = false;
	bool soundLoaded = false;
	bool playing = false;
	bool paused = false;
	bool looping = false;
	float volume = 1.0f;
	QString currentPath;
};

SystemAudio *SystemAudio::instance()
{
	static SystemAudio inst;
	return &inst;
}

SystemAudio::SystemAudio()
{
	impl = new Impl();

	// Initialize the audio engine
	ma_engine_config engineConfig = ma_engine_config_init();
	if (ma_engine_init(&engineConfig, &impl->engine) == MA_SUCCESS) {
		impl->engineInitialized = true;
	}
}

SystemAudio::~SystemAudio()
{
	stop();
	if (impl->engineInitialized) {
		ma_engine_uninit(&impl->engine);
	}
	delete impl;
}

void SystemAudio::play(const QString &path, float volume, bool loop, int64_t startTimeMs)
{
	if (!impl->engineInitialized)
		return;

	stop();

	impl->currentPath = path;
	impl->volume = volume;
	impl->looping = loop;

	// Convert QString to UTF-8 for miniaudio
	QByteArray pathUtf8 = path.toUtf8();

	// Initialize sound from file
	ma_uint32 flags = MA_SOUND_FLAG_DECODE; // Decode to memory for better seeking
	if (ma_sound_init_from_file(&impl->engine, pathUtf8.constData(), flags, NULL, NULL, &impl->sound) != MA_SUCCESS) {
		return;
	}

	impl->soundLoaded = true;

	// Set looping
	ma_sound_set_looping(&impl->sound, loop ? MA_TRUE : MA_FALSE);

	// Set volume
	ma_sound_set_volume(&impl->sound, volume);

	// Seek to start time if specified
	if (startTimeMs > 0) {
		// Get sample rate to convert ms to frames
		ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl->engine);
		ma_uint64 frameOffset = (startTimeMs * sampleRate) / 1000;
		ma_sound_seek_to_pcm_frame(&impl->sound, frameOffset);
	}

	// Start playback
	if (ma_sound_start(&impl->sound) == MA_SUCCESS) {
		impl->playing = true;
		impl->paused = false;
	}
}

void SystemAudio::stop()
{
	if (impl->soundLoaded) {
		ma_sound_stop(&impl->sound);
		ma_sound_uninit(&impl->sound);
		impl->soundLoaded = false;
	}
	impl->playing = false;
	impl->paused = false;
	impl->currentPath.clear();
}

void SystemAudio::pause()
{
	if (impl->playing && impl->soundLoaded && !impl->paused) {
		ma_sound_stop(&impl->sound);
		impl->paused = true;
		impl->playing = false;
	}
}

void SystemAudio::resume()
{
	if (impl->paused && impl->soundLoaded) {
		ma_sound_start(&impl->sound);
		impl->paused = false;
		impl->playing = true;
	}
}

void SystemAudio::setVolume(float volume)
{
	impl->volume = volume;
	if (impl->soundLoaded) {
		ma_sound_set_volume(&impl->sound, volume);
	}
}

void SystemAudio::seekTo(int64_t timeMs)
{
	if (!impl->soundLoaded)
		return;

	// Get sample rate to convert ms to frames
	ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl->engine);
	ma_uint64 frameOffset = (timeMs * sampleRate) / 1000;
	ma_sound_seek_to_pcm_frame(&impl->sound, frameOffset);

	// If we were playing, ensure we continue (seeking doesn't stop playback in miniaudio)
	// But if paused, stay paused
}

bool SystemAudio::isPlaying() const
{
	if (impl->soundLoaded && impl->playing) {
		// Also check if sound hasn't ended
		return !ma_sound_at_end(&impl->sound);
	}
	return false;
}

bool SystemAudio::isPaused() const
{
	return impl->paused;
}

int64_t SystemAudio::currentTime() const
{
	if (!impl->soundLoaded)
		return 0;

	ma_uint64 cursor;
	if (ma_sound_get_cursor_in_pcm_frames(&impl->sound, &cursor) != MA_SUCCESS)
		return 0;

	// Convert frames to milliseconds
	ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl->engine);
	if (sampleRate == 0)
		return 0;

	return (cursor * 1000) / sampleRate;
}

#endif // _WIN32
