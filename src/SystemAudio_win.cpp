#ifdef _WIN32

#include "SystemAudio.hpp"

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

struct SystemAudio::Impl {
	QString currentPath;
	bool looping = false;
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
	Q_UNUSED(volume); // Windows PlaySound doesn't support volume control

	stop();

	impl->currentPath = path;
	impl->looping = loop;

	DWORD flags = SND_FILENAME | SND_ASYNC;
	if (loop)
		flags |= SND_LOOP;

	std::wstring wpath = path.toStdWString();
	PlaySoundW(wpath.c_str(), NULL, flags);
}

void SystemAudio::stop()
{
	PlaySoundW(NULL, NULL, 0);
	impl->currentPath.clear();
	impl->looping = false;
}

#endif // _WIN32
