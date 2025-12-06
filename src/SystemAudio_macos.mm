#ifdef __APPLE__

#include "SystemAudio.hpp"

#import <AppKit/AppKit.h>

struct SystemAudio::Impl {
	NSSound *sound = nil;
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

	NSString *nsPath = [NSString stringWithUTF8String:path.toUtf8().constData()];
	impl->sound = [[NSSound alloc] initWithContentsOfFile:nsPath byReference:YES];

	if (impl->sound) {
		[impl->sound setVolume:volume];
		[impl->sound setLoops:loop ? YES : NO];
		[impl->sound play];
	}
}

void SystemAudio::stop()
{
	if (impl->sound) {
		[impl->sound stop];
		impl->sound = nil;
	}
}

#endif // __APPLE__
