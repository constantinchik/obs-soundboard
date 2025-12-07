#ifdef _WIN32

#include "SystemAudio.hpp"

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

struct SystemAudio::Impl {
	QString currentPath;
	bool playing = false;
	float volume = 1.0f;
};

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
	delete impl;
}

void SystemAudio::play(const QString &path, float volume, bool loop)
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

	// Play
	std::wstring playCmd = loop ? L"play soundboard repeat" : L"play soundboard";
	mciSendStringW(playCmd.c_str(), NULL, 0, NULL);

	impl->playing = true;
}

void SystemAudio::stop()
{
	if (impl->playing) {
		mciSendStringW(L"stop soundboard", NULL, 0, NULL);
		mciSendStringW(L"close soundboard", NULL, 0, NULL);
		impl->playing = false;
	}
	impl->currentPath.clear();
}

void SystemAudio::setVolume(float volume)
{
	if (impl->playing) {
		impl->volume = volume;
		int mciVolume = static_cast<int>(volume * 1000);
		std::wstring volCmd = L"setaudio soundboard volume to " + std::to_wstring(mciVolume);
		mciSendStringW(volCmd.c_str(), NULL, 0, NULL);
	}
}

bool SystemAudio::isPlaying() const
{
	return impl->playing;
}

#endif // _WIN32
