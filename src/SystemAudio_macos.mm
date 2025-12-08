#ifdef __APPLE__

#include "SystemAudio.hpp"

#import <AVFoundation/AVFoundation.h>

struct SystemAudio::Impl {
	AVPlayer *player = nil;
	AVPlayerItem *playerItem = nil;
	id timeObserver = nil;
	bool looping = false;
	bool paused = false;
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
	stop();

	NSString *nsPath = [NSString stringWithUTF8String:path.toUtf8().constData()];
	NSURL *url = [NSURL fileURLWithPath:nsPath];

	impl->playerItem = [AVPlayerItem playerItemWithURL:url];
	impl->player = [AVPlayer playerWithPlayerItem:impl->playerItem];
	impl->volume = volume;
	impl->looping = loop;
	impl->paused = false;

	impl->player.volume = volume;

	// Set up looping if needed
	if (loop) {
		impl->player.actionAtItemEnd = AVPlayerActionAtItemEndNone;
		[[NSNotificationCenter defaultCenter] addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
								  object:impl->playerItem
								   queue:nil
							      usingBlock:^(NSNotification *) {
								      [impl->player seekToTime:kCMTimeZero];
								      [impl->player play];
							      }];
	}

	// Seek to start time if specified
	if (startTimeMs > 0) {
		CMTime seekTime = CMTimeMakeWithSeconds(startTimeMs / 1000.0, NSEC_PER_SEC);
		[impl->player seekToTime:seekTime
			 toleranceBefore:kCMTimeZero
			  toleranceAfter:kCMTimeZero
		       completionHandler:^(BOOL finished) {
			       if (finished) {
				       [impl->player play];
			       }
		       }];
	} else {
		[impl->player play];
	}
}

void SystemAudio::stop()
{
	if (impl->player) {
		[impl->player pause];
		[[NSNotificationCenter defaultCenter] removeObserver:impl->player];
		impl->player = nil;
		impl->playerItem = nil;
		impl->paused = false;
	}
}

void SystemAudio::pause()
{
	if (impl->player && !impl->paused) {
		[impl->player pause];
		impl->paused = true;
	}
}

void SystemAudio::resume()
{
	if (impl->player && impl->paused) {
		[impl->player play];
		impl->paused = false;
	}
}

void SystemAudio::setVolume(float volume)
{
	impl->volume = volume;
	if (impl->player) {
		impl->player.volume = volume;
	}
}

void SystemAudio::seekTo(int64_t timeMs)
{
	if (impl->player) {
		CMTime seekTime = CMTimeMakeWithSeconds(timeMs / 1000.0, NSEC_PER_SEC);
		[impl->player seekToTime:seekTime toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
	}
}

bool SystemAudio::isPlaying() const
{
	return impl->player && impl->player.rate > 0 && !impl->paused;
}

bool SystemAudio::isPaused() const
{
	return impl->player && impl->paused;
}

int64_t SystemAudio::currentTime() const
{
	if (impl->player) {
		CMTime time = impl->player.currentTime;
		if (CMTIME_IS_VALID(time)) {
			return (int64_t)(CMTimeGetSeconds(time) * 1000.0);
		}
	}
	return 0;
}

#endif // __APPLE__
