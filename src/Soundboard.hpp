#pragma once

#include <QWidget>

#include <obs.hpp>

#include <QElapsedTimer>
#include <QHash>
#include <QPoint>
#include <QPointer>
#include <QTimer>

#include "components/SoundTileDelegate.hpp"

#include <memory>
#include <vector>

class MediaControls;
class MediaObj;
class QListWidgetItem;
class SceneTree;
class Ui_Soundboard;

class Soundboard : public QWidget {
	Q_OBJECT

private:
	QString prevPath;
	std::unique_ptr<Ui_Soundboard> ui;

	MediaObj *getCurrentMediaObj();
	QListWidgetItem *findItem(MediaObj *obj);

	OBSSourceAutoRelease source;
	OBSSignal volumeSignal;

	bool actionsEnabled = false;
	bool systemAudioEnabled = false;
	MediaObj *currentPlayingMedia = nullptr;

	QAction *renameMedia = nullptr;

	QTimer progressTimer;
	QElapsedTimer playStarted;

	/* Overlapping playbacks ("Play again" while something is already playing).
	 * Each one is a private copy of the main source on a free output channel,
	 * released as soon as it finishes. */
	struct Voice {
		QString uuid; // MediaObj UUID
		OBSSourceAutoRelease source;
		int channel = -1;
		int systemVoice = -1;
		quint64 seq = 0;
		QElapsedTimer started;
	};
	std::vector<std::unique_ptr<Voice>> voices;
	quint64 playSeq = 0;
	quint64 mainSeq = 0;
	QElapsedTimer mainStarted;

	int defaultClickAction = 0;     // SoundActionPlayAgain
	int defaultLongPressAction = 2; // SoundActionStop

	/* Click / long-press tracking, shared by the mouse and hotkeys */
	struct PressState {
		quint64 token = 0;
		bool longFired = false;
	};
	QHash<QString, PressState> presses;
	quint64 pressToken = 0;
	QString mousePressUuid;
	QPoint mousePressPos;

	static void onVolumeChanged(void *data, calldata_t *cd);

	void startProgressTracking();
	void setSystemAudioEnabled(bool enabled);

	bool isMainBusy();
	bool isMainPlaying(MediaObj *obj);
	bool isSoundPlaying(MediaObj *obj);
	void playNew(MediaObj *obj);
	void spawnVoice(MediaObj *obj);
	void stopSound(MediaObj *obj);
	void stopVoicesOf(const QString &uuid);
	void stopAllVoices();
	void releaseVoice(Voice &voice);
	void cleanupFinishedVoices();
	void syncVoiceSettings(obs_source_t *voice);
	float voiceVolume(MediaObj *obj);

	int effectiveClickAction(MediaObj *obj);
	int effectiveLongPressAction(MediaObj *obj);
	void performAction(MediaObj *obj, int action, bool longPress);
	void beginPress(const QString &uuid);
	void endPress(const QString &uuid, bool fireClick);
	void cancelPress(const QString &uuid);

	void updateItemDecorations(QListWidgetItem *item, MediaObj *obj);
	void updateAllItemDecorations();

private slots:
	void updatePlayingTile();
	void onSoundHotkeyPressed(MediaObj *obj);
	void onSoundHotkeyReleased(MediaObj *obj);
	void updateSystemAudioVolume();
	void onMediaStopClicked();
	void onMediaRestartClicked();
	void onMediaPauseClicked();
	void onMediaPlayClicked();
	void onMediaSeeked(int64_t timeMs);
	void on_actionAdd_triggered();
	void on_actionRemove_triggered();
	void on_actionEdit_triggered();
	void onSystemAudioToggled(bool checked);
	void updateActions();
	void on_list_customContextMenuRequested(const QPoint &pos);
	void on_actionDuplicate_triggered();

	MediaObj *add(const QString &name, const QString &path);
	void play(MediaObj *obj);

	void editMediaName();
	void mediaNameEdited(QWidget *editor);

	void itemRenamed(MediaObj *obj);

public:
	Soundboard(QWidget *parent = nullptr);
	~Soundboard();

	OBSDataArray saveMedia();
	void loadMedia(OBSDataArray array);
	void loadSource(OBSData saveData);
	void clear();

	void save(OBSData saveData);
	void load(OBSData saveData);

	void createSource();

protected:
	virtual bool eventFilter(QObject *obj, QEvent *event) override;
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;
};

class MediaRenameDelegate : public SoundTileDelegate {
	Q_OBJECT

public:
	MediaRenameDelegate(SceneTree *parent);
	virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;

protected:
	virtual bool eventFilter(QObject *editor, QEvent *event) override;
};
