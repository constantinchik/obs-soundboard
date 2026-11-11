#ifdef _WIN32

#include "SystemAudio.hpp"

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#include <set>
#include <string>

struct SystemAudio::Impl {
	QString currentPath;
	bool playing = false;
	bool paused = false;
	float volume = 1.0f;

	int nextVoiceId = 1;
	std::set<int> voices;
};

namespace {

std::wstring voiceAlias(int id)
{
	return L"sbvoice" + std::to_wstring(id);
}

void setMciVolume(const std::wstring &alias, float volume)
{
	int mciVolume = static_cast<int>(volume * 1000);
	std::wstring volCmd = L"setaudio " + alias + L" volume to " + std::to_wstring(mciVolume);
	mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
}

} // namespace

SystemAudio *SystemAudio::instance()
{
	static SystemAudio inst;
	return &inst;
}

SystemAudio::SystemAudio()
{
	impl = new Impl();
}

SystemAudio::~SystemAudio()
{
	stop();
	stopAllVoices();
	delete impl;
}

int SystemAudio::playVoice(const QString &path, float volume, bool loop)
{
	int id = impl->nextVoiceId++;
	std::wstring alias = voiceAlias(id);
	std::wstring wpath = path.toStdWString();

	std::wstring openCmd = L"open \"" + wpath + L"\" type mpegvideo alias " + alias;
	if (mciSendStringW(openCmd.c_str(), NULL, 0, NULL) != 0) {
		openCmd = L"open \"" + wpath + L"\" type waveaudio alias " + alias;
		if (mciSendStringW(openCmd.c_str(), NULL, 0, NULL) != 0)
			return -1;
	}

	setMciVolume(alias, volume);

	std::wstring playCmd = L"play " + alias + (loop ? L" repeat" : L"");
	mciSendStringW(playCmd.c_str(), NULL, 0, NULL);

	impl->voices.insert(id);
	return id;
}

void SystemAudio::stopVoice(int id)
{
	auto it = impl->voices.find(id);
	if (it == impl->voices.end())
		return;

	std::wstring alias = voiceAlias(id);
	mciSendStringW((L"stop " + alias).c_str(), NULL, 0, NULL);
	mciSendStringW((L"close " + alias).c_str(), NULL, 0, NULL);
	impl->voices.erase(it);
}

void SystemAudio::stopAllVoices()
{
	std::set<int> ids = impl->voices;
	for (int id : ids)
		stopVoice(id);
}

void SystemAudio::setVoiceVolume(int id, float volume)
{
	if (impl->voices.count(id))
		setMciVolume(voiceAlias(id), volume);
}

bool SystemAudio::isVoiceActive(int id) const
{
	if (!impl->voices.count(id))
		return false;

	wchar_t buffer[64] = {};
	std::wstring cmd = L"status " + voiceAlias(id) + L" mode";
	if (mciSendStringW(cmd.c_str(), buffer, 64, NULL) != 0)
		return false;

	return std::wstring(buffer) == L"playing";
}

void SystemAudio::play(const QString &path, float volume, bool loop, int64_t startTimeMs)
{
	stop();

	impl->currentPath = path;
	impl->volume = volume;

	// Open the audio file with MCI
	std::wstring wpath = path.toStdWString();
	std::wstring openCmd = L"open \"" + wpath + L"\" type mpegvideo alias soundboard";
	if (mciSendStringW(openCmd.c_str(), NULL, 0, NULL) != 0) {
		// Try waveaudio for wav files
		openCmd = L"open \"" + wpath + L"\" type waveaudio alias soundboard";
		if (mciSendStringW(openCmd.c_str(), NULL, 0, NULL) != 0)
			return;
	}

	// Set volume (MCI uses 0-1000 scale)
	int mciVolume = static_cast<int>(volume * 1000);
	std::wstring volCmd = L"setaudio soundboard volume to " + std::to_wstring(mciVolume);
	mciSendStringW(volCmd.c_str(), NULL, 0, NULL);

	// Seek to start time if specified
	if (startTimeMs > 0) {
		std::wstring seekCmd = L"seek soundboard to " + std::to_wstring(startTimeMs);
		mciSendStringW(seekCmd.c_str(), NULL, 0, NULL);
	}

	// Play
	std::wstring playCmd = loop ? L"play soundboard repeat" : L"play soundboard";
	mciSendStringW(playCmd.c_str(), NULL, 0, NULL);

	impl->playing = true;
}

void SystemAudio::stop()
{
	if (impl->playing || impl->paused) {
		mciSendStringW(L"stop soundboard", NULL, 0, NULL);
		mciSendStringW(L"close soundboard", NULL, 0, NULL);
		impl->playing = false;
		impl->paused = false;
	}
	impl->currentPath.clear();
}

void SystemAudio::pause()
{
	if (impl->playing && !impl->paused) {
		mciSendStringW(L"pause soundboard", NULL, 0, NULL);
		impl->paused = true;
		impl->playing = false;
	}
}

void SystemAudio::resume()
{
	if (impl->paused) {
		mciSendStringW(L"resume soundboard", NULL, 0, NULL);
		impl->paused = false;
		impl->playing = true;
	}
}

void SystemAudio::setVolume(float volume)
{
	if (impl->playing || impl->paused) {
		impl->volume = volume;
		int mciVolume = static_cast<int>(volume * 1000);
		std::wstring volCmd = L"setaudio soundboard volume to " + std::to_wstring(mciVolume);
		mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
	}
}

void SystemAudio::seekTo(int64_t timeMs)
{
	if (impl->playing || impl->paused) {
		std::wstring seekCmd = L"seek soundboard to " + std::to_wstring(timeMs);
		mciSendStringW(seekCmd.c_str(), NULL, 0, NULL);
		// After seek, MCI stops playback, so we need to resume if was playing
		if (impl->playing) {
			mciSendStringW(L"play soundboard", NULL, 0, NULL);
		}
	}
}

bool SystemAudio::isPlaying() const
{
	return impl->playing;
}

bool SystemAudio::isPaused() const
{
	return impl->paused;
}

int64_t SystemAudio::currentTime() const
{
	if (impl->playing || impl->paused) {
		wchar_t buffer[128];
		mciSendStringW(L"status soundboard position", buffer, 128, NULL);
		return _wtoi64(buffer);
	}
	return 0;
}

#endif // _WIN32
