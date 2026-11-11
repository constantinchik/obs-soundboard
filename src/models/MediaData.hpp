#pragma once

#include <obs.hpp>

#include <QObject>
#include <vector>

/* What a click or long press on a sound does while that sound is already playing.
 * An idle sound always starts playing (except a long press set to Stop). */
enum SoundAction : int {
	SoundActionDefault = -1, // per-sound only: use the soundboard-wide default
	SoundActionPlayAgain = 0, // start another overlapping instance
	SoundActionRestart = 1,   // stop this sound's instances and start from the beginning
	SoundActionStop = 2,      // stop all instances of this sound
};

class MediaObj : public QObject {
	Q_OBJECT

private:
	static std::vector<MediaObj *> mediaItems;

	QString uuid;

	QString name = "";
	QString path = "";
	bool loop = false;
	float volume = 1.0f;
	int clickAction = SoundActionDefault;
	int longPressAction = SoundActionDefault;

	obs_hotkey_id hotkey = OBS_INVALID_HOTKEY_ID;

private slots:
	void pressed();
	void released();

public:
	MediaObj(const QString &name, const QString &path);
	~MediaObj();

	static MediaObj *findByUUID(const QString &uuid);
	static MediaObj *findByName(const QString &name);

	QString getUUID();

	void setName(const QString &newName);
	QString getName();

	void setPath(const QString &newPath);
	QString getPath();

	obs_hotkey_id getHotkey();

	void setLoopEnabled(bool enable);
	bool loopEnabled();

	void setVolume(float volume);
	float getVolume();

	void setClickAction(int action) { clickAction = action; }
	int getClickAction() const { return clickAction; }

	void setLongPressAction(int action) { longPressAction = action; }
	int getLongPressAction() const { return longPressAction; }

signals:
	void hotkeyPressed(MediaObj *obj);
	void hotkeyReleased(MediaObj *obj);

	void renamed(MediaObj *obj);
};
