#if defined(__linux__) || defined(__FreeBSD__)

#include "SystemAudio.hpp"

#include <QProcess>

struct SystemAudio::Impl {
	QProcess *process = nullptr;
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
	Q_UNUSED(loop); // Loop not easily supported with subprocess

	stop();

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

#endif // __linux__ || __FreeBSD__
