#ifdef __APPLE__

#include "SystemAudio.hpp"

#import <AVFoundation/AVFoundation.h>

#include <map>
#include <vector>

struct SystemAudioVoice {
	AVPlayer *player = nil;
	id endObserver = nil;
	bool looping = false;
};

struct SystemAudio::Impl {
	AVPlayer *player = nil;
	AVPlayerItem *playerItem = nil;
	id timeObserver = nil;
	bool looping = false;
	bool paused = false;
	float volume = 1.0f;

	int nextVoiceId = 1;
	std::map<int, SystemAudioVoice> voices;
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
	stopAllVoices();
	delete impl;
}

int SystemAudio::playVoice(const QString &path, float volume, bool loop)
{
	NSString *nsPath = [NSString stringWithUTF8String:path.toUtf8().constData()];
	NSURL *url = [NSURL fileURLWithPath:nsPath];

	AVPlayerItem *item = [AVPlayerItem playerItemWithURL:url];
	AVPlayer *player = [AVPlayer playerWithPlayerItem:item];
	if (!player)
		return -1;

	player.volume = volume;

	SystemAudioVoice voice;
	voice.player = player;
	voice.looping = loop;

	if (loop) {
		player.actionAtItemEnd = AVPlayerActionAtItemEndNone;
		__weak AVPlayer *weakPlayer = player;
		voice.endObserver =
			[[NSNotificationCenter defaultCenter] addObserverForName:AVPlayerItemDidPlayToEndTimeNotification
									  object:item
									   queue:nil
								      usingBlock:^(NSNotification *) {
									      AVPlayer *strongPlayer = weakPlayer;
									      [strongPlayer seekToTime:kCMTimeZero];
									      [strongPlayer play];
								      }];
	}

	[player play];

	int voiceId = impl->nextVoiceId++;
	impl->voices[voiceId] = voice;
	return voiceId;
}

void SystemAudio::stopVoice(int voiceId)
{
	auto it = impl->voices.find(voiceId);
	if (it == impl->voices.end())
		return;

	[it->second.player pause];
	if (it->second.endObserver)
		[[NSNotificationCenter defaultCenter] removeObserver:it->second.endObserver];

	impl->voices.erase(it);
}

void SystemAudio::stopAllVoices()
{
	std::vector<int> ids;
	for (auto &pair : impl->voices)
		ids.push_back(pair.first);

	for (int voiceId : ids)
		stopVoice(voiceId);
}

void SystemAudio::setVoiceVolume(int voiceId, float volume)
{
	auto it = impl->voices.find(voiceId);
	if (it != impl->voices.end())
		it->second.player.volume = volume;
}

bool SystemAudio::isVoiceActive(int voiceId) const
{
	auto it = impl->voices.find(voiceId);
	if (it == impl->voices.end())
		return false;

	return it->second.looping || it->second.player.rate > 0;
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
