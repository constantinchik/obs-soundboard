#if defined(__linux__) || defined(__FreeBSD__)

#include "SystemAudio.hpp"

#include <QProcess>

struct SystemAudio::Impl {
	QProcess *process = nullptr;
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

void SystemAudio::play(const QString &path, float volume, bool loop, int64_t startTimeMs)
{
	Q_UNUSED(loop); // Loop not easily supported with subprocess
	Q_UNUSED(startTimeMs); // Seek not supported with subprocess

	stop();

	impl->volume = volume;
	impl->process = new QProcess();

	// Try paplay (PulseAudio) first, fall back to aplay (ALSA)
	QString program = "paplay";
	QStringList args;

	// Set volume (0.0 to 1.0 -> 0 to 65536 for paplay)
	int paVolume = static_cast<int>(volume * 65536);
	args << QString("--volume=%1").arg(paVolume) << path;

	impl->process->start(program, args);

	if (!impl->process->waitForStarted(100)) {
		// paplay failed, try aplay
		impl->process->deleteLater();
		impl->process = new QProcess();
		impl->process->start("aplay", QStringList() << path);
	}
}

void SystemAudio::stop()
{
	if (impl->process) {
		impl->process->kill();
		impl->process->waitForFinished(100);
		impl->process->deleteLater();
		impl->process = nullptr;
	}
}

void SystemAudio::pause()
{
	// Pause not supported with subprocess approach - just stop
	stop();
}

void SystemAudio::resume()
{
	// Resume not supported with subprocess approach
}

void SystemAudio::setVolume(float volume)
{
	// Volume change mid-play not supported with subprocess approach
	impl->volume = volume;
}

void SystemAudio::seekTo(int64_t timeMs)
{
	Q_UNUSED(timeMs);
	// Seek not supported with subprocess approach
}

bool SystemAudio::isPlaying() const
{
	return impl->process && impl->process->state() == QProcess::Running;
}

bool SystemAudio::isPaused() const
{
	return false; // Pause not supported
}

int64_t SystemAudio::currentTime() const
{
	return 0; // Time tracking not supported
}

#endif // __linux__ || __FreeBSD__
