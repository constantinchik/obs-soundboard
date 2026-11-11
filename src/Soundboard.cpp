#include "Soundboard.hpp"
#include "ui_Soundboard.h"

#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/util.hpp>

#include "plugin-support.h"

#include "components/SceneTree.hpp"
#include "components/MediaControls.hpp"
#include "dialogs/MediaEdit.hpp"
#include "models/MediaData.hpp"

#include "SystemAudio.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QObject>
#include <QToolButton>

#include "moc_Soundboard.cpp"

#define QT_UTF8(str) QString::fromUtf8(str, -1)
#define QT_TO_UTF8(str) str.toUtf8().constData()
#define QTStr(str) QString(obs_module_text(str))
#define MainStr(str) QString(obs_frontend_get_locale_string(str))

namespace {

QString getDefaultString(QString name = "")
{
	if (name.isEmpty())
		name = QTStr("Sound");

	if (!MediaObj::findByName(name))
		return name;

	int i = 2;

	for (;;) {
		QString out = name + " " + QString::number(i);

		if (!MediaObj::findByName(out))
			return out;

		i++;
	}
}

void onSave(obs_data_t *saveData, bool saving, void *data)
{
	Soundboard *sb = static_cast<Soundboard *>(data);

	if (saving)
		sb->save(saveData);
	else
		sb->load(saveData);
}

void onEvent(enum obs_frontend_event event, void *data)
{
	Soundboard *sb = static_cast<Soundboard *>(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		sb->createSource();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		sb->clear();
	default:
		break;
	};
}
} // namespace

Soundboard::Soundboard(QWidget *parent) : QWidget(parent), ui(new Ui_Soundboard)
{
	ui->setupUi(this);
	ui->list->SetGridItemWidth(104);
	ui->list->SetGridItemHeight(78);
	ui->list->SetGridMode(true);
	ui->list->SetPlaceholderText(QTStr("EmptySoundboard"));

	obs_frontend_add_event_callback(onEvent, this);
	obs_frontend_add_save_callback(onSave, this);

	ui->list->setItemDelegate(new MediaRenameDelegate(ui->list));

	progressTimer.setInterval(50);
	connect(&progressTimer, &QTimer::timeout, this, &Soundboard::updatePlayingTile);

	connect(ui->addButton, &QToolButton::clicked, ui->actionAdd, &QAction::trigger);

	// Clicks and long presses on tiles are detected here instead of itemClicked
	ui->list->viewport()->installEventFilter(this);

	renameMedia = new QAction(MainStr("Rename"), this);
	renameMedia->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameMedia, &QAction::triggered, this, &Soundboard::editMediaName);

#ifdef __APPLE__
	renameMedia->setShortcut({Qt::Key_Return});
#else
	renameMedia->setShortcut({Qt::Key_F2});
#endif

	addAction(renameMedia);

	connect(ui->list->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, &Soundboard::mediaNameEdited);

	// Connect media control signals for system audio
	connect(ui->mediaControls, &MediaControls::stopClicked, this, &Soundboard::onMediaStopClicked);
	connect(ui->mediaControls, &MediaControls::restartClicked, this, &Soundboard::onMediaRestartClicked);
	connect(ui->mediaControls, &MediaControls::pauseClicked, this, &Soundboard::onMediaPauseClicked);
	connect(ui->mediaControls, &MediaControls::playClicked, this, &Soundboard::onMediaPlayClicked);
	connect(ui->mediaControls, &MediaControls::seeked, this, &Soundboard::onMediaSeeked);
	connect(ui->monitorButton, &QToolButton::toggled, this, &Soundboard::onSystemAudioToggled);
}

Soundboard::~Soundboard()
{
	obs_frontend_remove_event_callback(onEvent, this);
	obs_frontend_remove_save_callback(onSave, this);
}

MediaObj *Soundboard::getCurrentMediaObj()
{
	QListWidgetItem *item = ui->list->currentItem();

	if (!item)
		return nullptr;

	QString uuid = item->data(Qt::UserRole).toString();
	return MediaObj::findByUUID(uuid);
}

QListWidgetItem *Soundboard::findItem(MediaObj *obj)
{
	for (int i = 0; i < ui->list->count(); i++) {
		QListWidgetItem *item = ui->list->item(i);
		QString uuid = item->data(Qt::UserRole).toString();

		if (uuid == obj->getUUID())
			return item;
	}

	return nullptr;
}

void Soundboard::createSource()
{
	if (obs_obj_invalid(source)) {
		source = obs_source_create("ffmpeg_source", obs_module_text("Soundboard"), nullptr, nullptr);
		obs_source_set_hidden(source, true);

		ui->mediaControls->SetSource(source.Get());
	}

	// Connect to volume changes for real-time system audio sync
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	volumeSignal.Connect(sh, "volume", onVolumeChanged, this);

	obs_set_output_source(63, source);
}

void Soundboard::onVolumeChanged(void *data, calldata_t *cd)
{
	Q_UNUSED(cd);
	Soundboard *sb = static_cast<Soundboard *>(data);
	QMetaObject::invokeMethod(sb, "updateSystemAudioVolume", Qt::QueuedConnection);
}

void Soundboard::updateSystemAudioVolume()
{
	// Keep overlapping voices in line with the main source's mixer volume
	for (auto &voice : voices) {
		syncVoiceSettings(voice->source);

		if (voice->systemVoice >= 0) {
			MediaObj *obj = MediaObj::findByUUID(voice->uuid);
			if (obj)
				SystemAudio::instance()->setVoiceVolume(voice->systemVoice, voiceVolume(obj));
		}
	}

	if (!systemAudioEnabled || !currentPlayingMedia)
		return;

	if (!SystemAudio::instance()->isPlaying())
		return;

	float soundVolume = currentPlayingMedia->getVolume();
	float sourceVolume = obs_source_get_volume(source);
	float combinedVolume = soundVolume * sourceVolume;

	SystemAudio::instance()->setVolume(combinedVolume);
}

void Soundboard::onMediaStopClicked()
{
	// The bar's stop button stops everything, including overlapping voices
	SystemAudio::instance()->stop();
	stopAllVoices();
	currentPlayingMedia = nullptr;
	updatePlayingTile();
}

void Soundboard::onMediaRestartClicked()
{
	startProgressTracking();

	if (!systemAudioEnabled || !currentPlayingMedia)
		return;

	QString path = currentPlayingMedia->getPath();
	float soundVolume = currentPlayingMedia->getVolume();
	float sourceVolume = obs_source_get_volume(source);
	float combinedVolume = soundVolume * sourceVolume;

	SystemAudio::instance()->play(path, combinedVolume, currentPlayingMedia->loopEnabled());
}

void Soundboard::onMediaPauseClicked()
{
	if (!systemAudioEnabled)
		return;

	SystemAudio::instance()->pause();
}

void Soundboard::onMediaPlayClicked()
{
	startProgressTracking();

	if (!systemAudioEnabled)
		return;

	SystemAudio::instance()->resume();
}

void Soundboard::onMediaSeeked(int64_t timeMs)
{
	if (!systemAudioEnabled || !currentPlayingMedia)
		return;

	// NSSound and other backends may not handle seeking while paused correctly
	// So we restart playback from the new position
	QString path = currentPlayingMedia->getPath();
	float soundVolume = currentPlayingMedia->getVolume();
	float sourceVolume = obs_source_get_volume(source);
	float combinedVolume = soundVolume * sourceVolume;

	SystemAudio::instance()->play(path, combinedVolume, currentPlayingMedia->loopEnabled(), timeMs);
}

OBSDataArray Soundboard::saveMedia()
{
	OBSDataArrayAutoRelease array = obs_data_array_create();

	for (int i = 0; i < ui->list->count(); i++) {
		QListWidgetItem *item = ui->list->item(i);
		QString uuid = item->data(Qt::UserRole).toString();

		MediaObj *obj = MediaObj::findByUUID(uuid);

		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "name", QT_TO_UTF8(obj->getName()));
		obs_data_set_string(settings, "path", QT_TO_UTF8(obj->getPath()));
		obs_data_set_bool(settings, "loop", obj->loopEnabled());
		obs_data_set_double(settings, "volume", (double)obj->getVolume());
		obs_data_set_int(settings, "click_action", obj->getClickAction());
		obs_data_set_int(settings, "long_press_action", obj->getLongPressAction());

		OBSDataArrayAutoRelease hotkeyArray = obs_hotkey_save(obj->getHotkey());
		obs_data_set_array(settings, "sound_hotkey", hotkeyArray);

		obs_data_array_push_back(array, settings);
	};

	return array.Get();
}

void Soundboard::loadMedia(OBSDataArray array)
{
	for (size_t i = 0; i < obs_data_array_count(array); i++) {
		OBSDataAutoRelease settings = obs_data_array_item(array, i);

		obs_data_set_default_string(settings, "name", obs_module_text("Sound"));
		obs_data_set_default_double(settings, "volume", 1.0);
		obs_data_set_default_int(settings, "click_action", SoundActionDefault);
		obs_data_set_default_int(settings, "long_press_action", SoundActionDefault);

		QString name = obs_data_get_string(settings, "name");
		QString path = obs_data_get_string(settings, "path");
		bool loop = obs_data_get_bool(settings, "loop");
		float volume = (float)obs_data_get_double(settings, "volume");

		OBSDataArrayAutoRelease hotkeyArray = obs_data_get_array(settings, "sound_hotkey");

		MediaObj *obj = add(name, path);
		obs_hotkey_load(obj->getHotkey(), hotkeyArray);
		obj->setLoopEnabled(loop);
		obj->setVolume(volume);
		obj->setClickAction((int)obs_data_get_int(settings, "click_action"));
		obj->setLongPressAction((int)obs_data_get_int(settings, "long_press_action"));
	}

	updateAllItemDecorations();
}

void Soundboard::save(OBSData saveData)
{
	QMainWindow *window = (QMainWindow *)obs_frontend_get_main_window();
	QDockWidget *dock = static_cast<QDockWidget *>(parent());

	OBSDataArray array = saveMedia();
	obs_data_set_array(saveData, "soundboard_array", array);

	if (!obs_obj_invalid(source)) {
		OBSDataAutoRelease sourceData = obs_save_source(source);
		obs_data_set_obj(saveData, "soundboard_source", sourceData);
	}

	obs_data_set_bool(saveData, "dock_visible", dock->isVisible());
	obs_data_set_bool(saveData, "dock_floating", dock->isFloating());
	obs_data_set_string(saveData, "dock_geometry", dock->saveGeometry().toBase64().constData());
	obs_data_set_int(saveData, "dock_area", window->dockWidgetArea(dock));
	obs_data_set_bool(saveData, "grid_mode", ui->list->GetGridMode());

	MediaObj *obj = getCurrentMediaObj();

	if (obj)
		obs_data_set_string(saveData, "current_sound", QT_TO_UTF8(obj->getName()));

	obs_data_set_bool(saveData, "use_countdown", ui->mediaControls->countDownTimer);
	obs_data_set_bool(saveData, "system_audio_enabled", systemAudioEnabled);
	obs_data_set_int(saveData, "default_click_action", defaultClickAction);
	obs_data_set_int(saveData, "default_long_press_action", defaultLongPressAction);
}

void Soundboard::loadSource(OBSData saveData)
{
	OBSDataAutoRelease sourceData = obs_data_get_obj(saveData, "soundboard_source");

	if (sourceData) {
		obs_data_set_obj(sourceData, "settings", nullptr);
		source = obs_load_source(sourceData);
		obs_source_set_hidden(source, true);

		if (obs_obj_invalid(source))
			return;

		ui->mediaControls->SetSource(source.Get());
	}
}

void Soundboard::load(OBSData saveData)
{
	QMainWindow *window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QDockWidget *dock = window->findChild<QDockWidget *>("SoundboardDock");

	loadSource(saveData);

	obs_data_set_default_int(saveData, "default_click_action", SoundActionPlayAgain);
	obs_data_set_default_int(saveData, "default_long_press_action", SoundActionStop);
	defaultClickAction = (int)obs_data_get_int(saveData, "default_click_action");
	defaultLongPressAction = (int)obs_data_get_int(saveData, "default_long_press_action");

	OBSDataArrayAutoRelease array = obs_data_get_array(saveData, "soundboard_array");
	loadMedia(array.Get());

	const char *geometry = obs_data_get_string(saveData, "dock_geometry");

	if (geometry && *geometry)
		dock->restoreGeometry(QByteArray::fromBase64(QByteArray(geometry)));

	const auto dockArea = static_cast<Qt::DockWidgetArea>(obs_data_get_int(saveData, "dock_area"));

	if (dockArea)
		window->addDockWidget(dockArea, dock);

	bool visible = obs_data_get_bool(saveData, "dock_visible");
	dock->setVisible(visible);

	obs_data_set_default_bool(saveData, "dock_floating", true);
	bool floating = obs_data_get_bool(saveData, "dock_floating");
	dock->setFloating(floating);

	obs_data_set_default_bool(saveData, "grid_mode", true);
	bool grid = obs_data_get_bool(saveData, "grid_mode");
	ui->list->SetGridMode(grid);

	QString lastSound = obs_data_get_string(saveData, "current_sound");

	if (!lastSound.isEmpty()) {
		MediaObj *obj = MediaObj::findByName(lastSound);
		QListWidgetItem *item = findItem(obj);

		if (item)
			ui->list->setCurrentItem(item);
		else
			ui->list->setCurrentRow(0);
	} else {
		ui->list->setCurrentRow(0);
	}

	bool countdown = obs_data_get_bool(saveData, "use_countdown");
	ui->mediaControls->countDownTimer = countdown;

	setSystemAudioEnabled(obs_data_get_bool(saveData, "system_audio_enabled"));
}

void Soundboard::setSystemAudioEnabled(bool enabled)
{
	systemAudioEnabled = enabled;

	// Checked = sounds also play through your own output (monitoring on)
	QSignalBlocker blocker(ui->monitorButton);
	ui->monitorButton->setChecked(enabled);
	ui->monitorButton->setText(enabled ? QStringLiteral("🎧") : QStringLiteral("🔇"));
	ui->monitorButton->setToolTip(enabled ? QTStr("SystemAudioOn") : QTStr("SystemAudioOff"));
}

void Soundboard::clear()
{
	ui->mediaControls->countDownTimer = false;
	ui->mediaControls->SetSource(nullptr);
	volumeSignal.Disconnect();
	source = nullptr;

	stopAllVoices();
	presses.clear();
	mousePressUuid.clear();

	SystemAudio::instance()->stop();
	setSystemAudioEnabled(false);
	currentPlayingMedia = nullptr;
	progressTimer.stop();

	prevPath = "";

	for (int i = 0; i < ui->list->count(); i++) {
		QListWidgetItem *item = ui->list->item(i);
		QString uuid = item->data(Qt::UserRole).toString();

		MediaObj *obj = MediaObj::findByUUID(uuid);
		delete obj;
		obj = nullptr;
	}

	ui->list->clear();

	updateActions();
}

void Soundboard::startProgressTracking()
{
	playStarted.start();

	if (!progressTimer.isActive())
		progressTimer.start();

	updatePlayingTile();
}

namespace {

constexpr int startGraceMs = 1000; // a just-started source may not report "playing" yet
constexpr int longPressMs = 500;
constexpr int firstVoiceChannel = 62; // channel 63 is the main soundboard source
constexpr int lastVoiceChannel = 40;

bool isActiveState(obs_media_state state)
{
	return state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_PAUSED ||
	       state == OBS_MEDIA_STATE_OPENING || state == OBS_MEDIA_STATE_BUFFERING;
}

double sourceProgress(obs_source_t *src)
{
	obs_media_state state = obs_source_media_get_state(src);
	if (state != OBS_MEDIA_STATE_PLAYING && state != OBS_MEDIA_STATE_PAUSED)
		return 0.0;

	int64_t duration = obs_source_media_get_duration(src);
	int64_t time = obs_source_media_get_time(src);
	return duration > 0 ? (double)time / (double)duration : 0.0;
}

} // namespace

bool Soundboard::isMainBusy()
{
	if (!currentPlayingMedia || obs_obj_invalid(source))
		return false;

	if (isActiveState(obs_source_media_get_state(source)))
		return true;

	return mainStarted.isValid() && mainStarted.elapsed() < startGraceMs;
}

bool Soundboard::isMainPlaying(MediaObj *obj)
{
	return obj && currentPlayingMedia == obj && isMainBusy();
}

bool Soundboard::isSoundPlaying(MediaObj *obj)
{
	if (isMainPlaying(obj))
		return true;

	QString uuid = obj->getUUID();
	for (auto &voice : voices) {
		if (voice->uuid == uuid)
			return true;
	}

	return false;
}

float Soundboard::voiceVolume(MediaObj *obj)
{
	float sourceVolume = obs_obj_invalid(source) ? 1.0f : obs_source_get_volume(source);
	return obj->getVolume() * sourceVolume;
}

void Soundboard::syncVoiceSettings(obs_source_t *voice)
{
	if (!voice || obs_obj_invalid(source))
		return;

	obs_source_set_volume(voice, obs_source_get_volume(source));
	obs_source_set_muted(voice, obs_source_muted(source));
	obs_source_set_balance_value(voice, obs_source_get_balance_value(source));
	obs_source_set_audio_mixers(voice, obs_source_get_audio_mixers(source));

	obs_monitoring_type monitoring = obs_source_get_monitoring_type(source);
	if (obs_source_get_monitoring_type(voice) != monitoring)
		obs_source_set_monitoring_type(voice, monitoring);
}

void Soundboard::spawnVoice(MediaObj *obj)
{
	int channel = -1;

	for (int attempt = 0; attempt < 2 && channel < 0; attempt++) {
		for (int ch = firstVoiceChannel; ch >= lastVoiceChannel; ch--) {
			OBSSourceAutoRelease existing = obs_get_output_source((uint32_t)ch);
			if (!existing) {
				channel = ch;
				break;
			}
		}

		// Every channel taken: drop the oldest voice and try again
		if (channel < 0 && !voices.empty()) {
			releaseVoice(*voices.front());
			voices.erase(voices.begin());
		}
	}

	if (channel < 0) {
		blog(LOG_WARNING, "[Soundboard] No free output channel for an overlapping sound");
		return;
	}

	QString path = obj->getPath();

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "looping", obj->loopEnabled());
	obs_data_set_string(settings, "local_file", QT_TO_UTF8(path));
	obs_data_set_bool(settings, "is_local_file", true);

	auto voice = std::make_unique<Voice>();
	voice->uuid = obj->getUUID();
	voice->source = obs_source_create_private("ffmpeg_source", "Soundboard voice", settings);

	if (obs_obj_invalid(voice->source))
		return;

	syncVoiceSettings(voice->source);
	if (!obs_obj_invalid(source))
		obs_source_copy_filters(voice->source, source);

	voice->channel = channel;
	obs_set_output_source((uint32_t)channel, voice->source);

	if (systemAudioEnabled)
		voice->systemVoice = SystemAudio::instance()->playVoice(path, voiceVolume(obj), obj->loopEnabled());

	voice->seq = ++playSeq;
	voice->started.start();
	voices.push_back(std::move(voice));

	startProgressTracking();
}

void Soundboard::releaseVoice(Voice &voice)
{
	if (voice.channel >= 0) {
		OBSSourceAutoRelease current = obs_get_output_source((uint32_t)voice.channel);
		if (current.Get() == voice.source.Get())
			obs_set_output_source((uint32_t)voice.channel, nullptr);
		voice.channel = -1;
	}

	if (voice.systemVoice >= 0) {
		SystemAudio::instance()->stopVoice(voice.systemVoice);
		voice.systemVoice = -1;
	}

	voice.source = nullptr;
}

void Soundboard::stopVoicesOf(const QString &uuid)
{
	for (auto it = voices.begin(); it != voices.end();) {
		if ((*it)->uuid == uuid) {
			releaseVoice(**it);
			it = voices.erase(it);
		} else {
			++it;
		}
	}
}

void Soundboard::stopAllVoices()
{
	for (auto &voice : voices)
		releaseVoice(*voice);

	voices.clear();
	SystemAudio::instance()->stopAllVoices();
}

void Soundboard::cleanupFinishedVoices()
{
	for (auto it = voices.begin(); it != voices.end();) {
		Voice &voice = **it;
		bool active = !obs_obj_invalid(voice.source) && isActiveState(obs_source_media_get_state(voice.source));

		if (!active && voice.started.elapsed() < startGraceMs)
			active = true;

		if (!active) {
			releaseVoice(voice);
			it = voices.erase(it);
		} else {
			++it;
		}
	}
}

void Soundboard::stopSound(MediaObj *obj)
{
	stopVoicesOf(obj->getUUID());

	if (isMainPlaying(obj)) {
		obs_source_media_stop(source);
		SystemAudio::instance()->stop();
		currentPlayingMedia = nullptr;
	}

	updatePlayingTile();
}

void Soundboard::playNew(MediaObj *obj)
{
	// The main source (with the media bar) is used whenever it's free;
	// overlapping sounds get their own temporary voice.
	if (!isMainBusy())
		play(obj);
	else
		spawnVoice(obj);
}

int Soundboard::effectiveClickAction(MediaObj *obj)
{
	int action = obj->getClickAction();
	return action == SoundActionDefault ? defaultClickAction : action;
}

int Soundboard::effectiveLongPressAction(MediaObj *obj)
{
	int action = obj->getLongPressAction();
	return action == SoundActionDefault ? defaultLongPressAction : action;
}

void Soundboard::performAction(MediaObj *obj, int action, bool longPress)
{
	if (!obj)
		return;

	cleanupFinishedVoices();

	if (!isSoundPlaying(obj)) {
		// An idle sound always starts, except a long press that means "stop"
		if (!(longPress && action == SoundActionStop))
			playNew(obj);
		return;
	}

	switch (action) {
	case SoundActionRestart: {
		bool inMain = isMainPlaying(obj);
		stopVoicesOf(obj->getUUID());

		if (inMain)
			play(obj); // same file in the main source: restarts it
		else
			playNew(obj);
		break;
	}
	case SoundActionStop:
		stopSound(obj);
		break;
	case SoundActionPlayAgain:
	default:
		playNew(obj);
		break;
	}
}

void Soundboard::beginPress(const QString &uuid)
{
	quint64 token = ++pressToken;
	presses[uuid] = PressState{token, false};

	QTimer::singleShot(longPressMs, this, [this, uuid, token]() {
		auto it = presses.find(uuid);
		if (it == presses.end() || it->token != token)
			return;

		it->longFired = true;

		MediaObj *obj = MediaObj::findByUUID(uuid);
		if (obj)
			performAction(obj, effectiveLongPressAction(obj), true);
	});
}

void Soundboard::endPress(const QString &uuid, bool fireClick)
{
	auto it = presses.find(uuid);
	if (it == presses.end())
		return;

	bool longFired = it->longFired;
	presses.erase(it);

	if (longFired || !fireClick)
		return;

	MediaObj *obj = MediaObj::findByUUID(uuid);
	if (obj)
		performAction(obj, effectiveClickAction(obj), false);
}

void Soundboard::cancelPress(const QString &uuid)
{
	presses.remove(uuid);
}

void Soundboard::onSoundHotkeyPressed(MediaObj *obj)
{
	beginPress(obj->getUUID());
}

void Soundboard::onSoundHotkeyReleased(MediaObj *obj)
{
	endPress(obj->getUUID(), true);
}

bool Soundboard::eventFilter(QObject *watched, QEvent *event)
{
	if (watched != ui->list->viewport())
		return QWidget::eventFilter(watched, event);

	switch (event->type()) {
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonDblClick: {
		// Qt delivers the second click of a fast double click as DblClick
		QMouseEvent *me = static_cast<QMouseEvent *>(event);
		if (me->button() != Qt::LeftButton)
			break;

		if (!mousePressUuid.isEmpty())
			cancelPress(mousePressUuid);
		mousePressUuid.clear();

		QPoint pos = me->position().toPoint();
		QListWidgetItem *item = ui->list->itemAt(pos);

		if (item) {
			mousePressUuid = item->data(Qt::UserRole).toString();
			mousePressPos = pos;
			beginPress(mousePressUuid);
		}
		break;
	}
	case QEvent::MouseMove: {
		if (mousePressUuid.isEmpty())
			break;

		// Moving far enough starts a drag to reorder; that's not a click
		QMouseEvent *me = static_cast<QMouseEvent *>(event);
		if ((me->position().toPoint() - mousePressPos).manhattanLength() >= QApplication::startDragDistance()) {
			cancelPress(mousePressUuid);
			mousePressUuid.clear();
		}
		break;
	}
	case QEvent::MouseButtonRelease: {
		QMouseEvent *me = static_cast<QMouseEvent *>(event);
		if (me->button() != Qt::LeftButton || mousePressUuid.isEmpty())
			break;

		QListWidgetItem *item = ui->list->itemAt(me->position().toPoint());
		bool sameTile = item && item->data(Qt::UserRole).toString() == mousePressUuid;

		QString uuid = mousePressUuid;
		mousePressUuid.clear();
		endPress(uuid, sameTile);
		break;
	}
	default:
		break;
	}

	return QWidget::eventFilter(watched, event);
}

void Soundboard::updateItemDecorations(QListWidgetItem *item, MediaObj *obj)
{
	if (!item || !obj)
		return;

	int click = obj->getClickAction();
	int longPress = obj->getLongPressAction();
	bool overridden = click != SoundActionDefault || longPress != SoundActionDefault;

	QString tooltip = obj->getName();
	if (click != SoundActionDefault)
		tooltip += "\n" + QTStr("ClickAction") + ": " + MediaEdit::actionName(click);
	if (longPress != SoundActionDefault)
		tooltip += "\n" + QTStr("LongPressAction") + ": " + MediaEdit::actionName(longPress);

	item->setToolTip(tooltip);

	if (item->data(SoundTileDelegate::OverrideRole).toBool() != overridden)
		item->setData(SoundTileDelegate::OverrideRole, overridden);
}

void Soundboard::updateAllItemDecorations()
{
	for (int i = 0; i < ui->list->count(); i++) {
		QListWidgetItem *item = ui->list->item(i);
		updateItemDecorations(item, MediaObj::findByUUID(item->data(Qt::UserRole).toString()));
	}
}

void Soundboard::updatePlayingTile()
{
	cleanupFinishedVoices();

	const bool mainBusy = isMainBusy();
	const QString mainUuid = (mainBusy && currentPlayingMedia) ? currentPlayingMedia->getUUID() : QString();

	for (int i = 0; i < ui->list->count(); i++) {
		QListWidgetItem *item = ui->list->item(i);
		QString uuid = item->data(Qt::UserRole).toString();

		// Show the most recently started instance of this sound
		obs_source_t *latest = nullptr;
		quint64 latestSeq = 0;
		bool isPlaying = false;

		if (mainBusy && uuid == mainUuid) {
			latest = source;
			latestSeq = mainSeq;
			isPlaying = true;
		}

		for (auto &voice : voices) {
			if (voice->uuid == uuid && voice->seq >= latestSeq) {
				latest = voice->source;
				latestSeq = voice->seq;
				isPlaying = true;
			}
		}

		double itemProgress = (isPlaying && latest) ? sourceProgress(latest) : 0.0;

		if (item->data(SoundTileDelegate::PlayingRole).toBool() != isPlaying)
			item->setData(SoundTileDelegate::PlayingRole, isPlaying);

		if (item->data(SoundTileDelegate::ProgressRole).toDouble() != itemProgress)
			item->setData(SoundTileDelegate::ProgressRole, itemProgress);
	}

	if (!mainBusy && voices.empty())
		progressTimer.stop();
}

void Soundboard::play(MediaObj *obj)
{
	if (!obj)
		return;

	QString path = obj->getPath();
	currentPlayingMedia = obj;
	mainSeq = ++playSeq;
	mainStarted.start();
	startProgressTracking();

	// Calculate combined volume: per-sound volume × source mixer volume
	float soundVolume = obj->getVolume();
	float sourceVolume = obs_source_get_volume(source);
	float combinedVolume = soundVolume * sourceVolume;

	if (prevPath == path) {
		obs_source_media_restart(source);

		if (systemAudioEnabled)
			SystemAudio::instance()->play(path, combinedVolume, obj->loopEnabled());

		return;
	}

	prevPath = path;

	QListWidgetItem *item = findItem(obj);

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "looping", obj->loopEnabled());
	obs_data_set_string(settings, "local_file", QT_TO_UTF8(path));
	obs_data_set_bool(settings, "is_local_file", true);
	obs_source_update(source, settings);

	if (systemAudioEnabled)
		SystemAudio::instance()->play(path, combinedVolume, obj->loopEnabled());

	ui->list->setCurrentItem(item);
}

void Soundboard::onSystemAudioToggled(bool checked)
{
	setSystemAudioEnabled(checked);

	if (!systemAudioEnabled) {
		SystemAudio::instance()->stop();
		SystemAudio::instance()->stopAllVoices();
		for (auto &voice : voices)
			voice->systemVoice = -1;
	} else {
		// If monitoring was just enabled and OBS is playing, sync system audio
		if (currentPlayingMedia && !obs_obj_invalid(source)) {
			obs_media_state state = obs_source_media_get_state(source);
			if (state == OBS_MEDIA_STATE_PLAYING) {
				QString path = currentPlayingMedia->getPath();
				float soundVolume = currentPlayingMedia->getVolume();
				float sourceVolume = obs_source_get_volume(source);
				float combinedVolume = soundVolume * sourceVolume;
				int64_t currentTimeMs = obs_source_media_get_time(source);

				// Start playback from current position
				SystemAudio::instance()->play(path, combinedVolume, currentPlayingMedia->loopEnabled(), currentTimeMs);
			}
		}
	}
}

void Soundboard::itemRenamed(MediaObj *obj)
{
	QListWidgetItem *item = findItem(obj);

	if (item) {
		item->setText(obj->getName());
		updateItemDecorations(item, obj);
	}
}

MediaObj *Soundboard::add(const QString &name_, const QString &path)
{
	QString name = getDefaultString(name_);

	MediaObj *obj = new MediaObj(name, path);

	QListWidgetItem *item = new QListWidgetItem(name);
	item->setData(Qt::UserRole, obj->getUUID());
	ui->list->addItem(item);
	ui->list->setCurrentItem(item);
	updateItemDecorations(item, obj);

	connect(obj, &MediaObj::hotkeyPressed, this, &Soundboard::onSoundHotkeyPressed);
	connect(obj, &MediaObj::hotkeyReleased, this, &Soundboard::onSoundHotkeyReleased);
	connect(obj, &MediaObj::renamed, this, &Soundboard::itemRenamed);

	updateActions();

	return obj;
}

void Soundboard::on_actionAdd_triggered()
{
	MediaEdit edit(this);

	auto added = [&, this]() {
		QString name = edit.getName();
		QString path = edit.getPath();
		bool loop = edit.loopChecked();

		MediaObj *obj = add(name, path);
		obj->setLoopEnabled(loop);
		obj->setClickAction(edit.clickAction());
		obj->setLongPressAction(edit.longPressAction());
		updateItemDecorations(findItem(obj), obj);
	};

	connect(&edit, &QDialog::accepted, this, added);

	edit.setName(getDefaultString());
	edit.setDefaultActions(defaultClickAction, defaultLongPressAction);
	edit.exec();
}

void Soundboard::on_actionEdit_triggered()
{
	MediaObj *obj = getCurrentMediaObj();

	if (!obj)
		return;

	MediaEdit edit(this);

	auto edited = [&]() {
		QString name = edit.getName();
		QString path = edit.getPath();
		bool loop = edit.loopChecked();

		obj->setName(name);
		obj->setPath(path);
		obj->setLoopEnabled(loop);
		obj->setClickAction(edit.clickAction());
		obj->setLongPressAction(edit.longPressAction());
		updateItemDecorations(findItem(obj), obj);
	};

	connect(&edit, &QDialog::accepted, this, edited);

	edit.setName(obj->getName());
	edit.setPath(obj->getPath());
	edit.setLoopChecked(obj->loopEnabled());
	edit.setDefaultActions(defaultClickAction, defaultLongPressAction);
	edit.setClickAction(obj->getClickAction());
	edit.setLongPressAction(obj->getLongPressAction());
	edit.exec();

	prevPath = "";
}

void Soundboard::updateActions()
{
	bool enable = ui->list->count() > 0;

	if (actionsEnabled == enable)
		return;

	ui->actionRemove->setEnabled(enable);
	ui->actionEdit->setEnabled(enable);

	actionsEnabled = enable;
}

void Soundboard::on_actionRemove_triggered()
{
	MediaObj *obj = getCurrentMediaObj();

	if (!obj)
		return;

	QString name = obj->getName();

	QMessageBox::StandardButton reply = QMessageBox::question(this, MainStr("ConfirmRemove.Title"),
								  MainStr("ConfirmRemove.Text").arg(name),
								  QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::No)
		return;

	if (obj == currentPlayingMedia)
		currentPlayingMedia = nullptr;

	stopVoicesOf(obj->getUUID());
	presses.remove(obj->getUUID());

	QListWidgetItem *item = findItem(obj);
	delete ui->list->takeItem(ui->list->row(item));
	obj->deleteLater();

	updateActions();
}

void Soundboard::on_actionDuplicate_triggered()
{
	MediaObj *obj = getCurrentMediaObj();

	if (!obj)
		return;

	QString name = getDefaultString(obj->getName());
	QString path = obj->getPath();
	bool loop = obj->loopEnabled();
	MediaObj *newObj = add(name, path);
	newObj->setLoopEnabled(loop);
	newObj->setVolume(obj->getVolume());
	newObj->setClickAction(obj->getClickAction());
	newObj->setLongPressAction(obj->getLongPressAction());
	updateItemDecorations(findItem(newObj), newObj);
}

void Soundboard::on_list_customContextMenuRequested(const QPoint &pos)
{
	QListWidgetItem *item = ui->list->itemAt(pos);

	QMenu popup(this);

	popup.addAction(ui->actionAdd);
	popup.addAction(MainStr("Basic.Filters"), this, [this]() { obs_frontend_open_source_filters(source); });
	popup.addSeparator();

	if (item) {
		popup.addAction(renameMedia);
		popup.addSeparator();
		popup.addAction(ui->actionEdit);
		popup.addAction(ui->actionRemove);
		popup.addAction(ui->actionDuplicate);
		popup.addSeparator();
	}

	popup.addSeparator();

	QMenu subMenu(MainStr("Basic.Main.ListMode"));
	QAction *listAction = subMenu.addAction(MainStr("List"), this, [this]() { ui->list->SetGridMode(false); });
	listAction->setCheckable(true);
	QAction *gridAction = subMenu.addAction(MainStr("Grid"), this, [this]() { ui->list->SetGridMode(true); });
	gridAction->setCheckable(true);

	bool grid = ui->list->GetGridMode();

	if (grid)
		gridAction->setChecked(true);
	else
		listAction->setChecked(true);

	popup.addMenu(&subMenu);

	// Soundboard-wide defaults; tiles can override them in their properties
	const int actions[] = {SoundActionPlayAgain, SoundActionRestart, SoundActionStop};

	QMenu clickMenu(QTStr("DefaultClickAction"));
	QMenu longPressMenu(QTStr("DefaultLongPressAction"));

	for (int action : actions) {
		QAction *clickItem = clickMenu.addAction(MediaEdit::actionName(action), this, [this, action]() {
			defaultClickAction = action;
			updateAllItemDecorations();
		});
		clickItem->setCheckable(true);
		clickItem->setChecked(defaultClickAction == action);

		QAction *longItem = longPressMenu.addAction(MediaEdit::actionName(action), this, [this, action]() {
			defaultLongPressAction = action;
			updateAllItemDecorations();
		});
		longItem->setCheckable(true);
		longItem->setChecked(defaultLongPressAction == action);
	}

	popup.addMenu(&clickMenu);
	popup.addMenu(&longPressMenu);

	popup.exec(QCursor::pos());
}

void Soundboard::dragEnterEvent(QDragEnterEvent *event)
{
	// refuse drops of our own widgets
	if (event->source() != nullptr) {
		event->setDropAction(Qt::IgnoreAction);
		return;
	}

	if (event->mimeData()->hasUrls())
		event->acceptProposedAction();
}

void Soundboard::dragLeaveEvent(QDragLeaveEvent *event)
{
	event->accept();
}

void Soundboard::dragMoveEvent(QDragMoveEvent *event)
{
	event->acceptProposedAction();
}

void Soundboard::dropEvent(QDropEvent *event)
{
	QStringList supportedExt;
	supportedExt << "mp3"
		     << "aac"
		     << "ogg"
		     << "wav"
		     << "flac";

	foreach(const QUrl &url, event->mimeData()->urls())
	{
		QString path = url.toLocalFile();
		QFileInfo fi(path);
		QString name = fi.fileName();
		QString ext = fi.suffix();

		if (!supportedExt.contains(ext))
			continue;

		add(name, path);
	}
}

void Soundboard::editMediaName()
{
	removeAction(renameMedia);
	QListWidgetItem *item = ui->list->currentItem();
	Qt::ItemFlags flags = item->flags();

	item->setFlags(flags | Qt::ItemIsEditable);
	ui->list->editItem(item);
	item->setFlags(flags);
}

void Soundboard::mediaNameEdited(QWidget *editor)
{
	addAction(renameMedia);
	MediaObj *obj = getCurrentMediaObj();
	QLineEdit *edit = qobject_cast<QLineEdit *>(editor);
	QString name = edit->text().trimmed();

	if (!obj)
		return;

	QListWidgetItem *item = findItem(obj);
	QString origName = obj->getName();

	if (name == origName) {
		item->setText(origName);
		return;
	}

	if (name.isEmpty()) {
		item->setText(origName);
		QMessageBox::warning(this, MainStr("EmptyName.Title"), MainStr("EmptyName.Text"));
		return;
	}

	if (MediaObj::findByName(name)) {
		item->setText(origName);
		QMessageBox::warning(this, MainStr("NameExists.Title"), MainStr("NameExists.Text"));
		return;
	}

	obj->setName(name);
}

MediaRenameDelegate::MediaRenameDelegate(SceneTree *parent) : SoundTileDelegate(parent) {}

void MediaRenameDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	QStyledItemDelegate::setEditorData(editor, index);
	QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
	if (lineEdit)
		lineEdit->selectAll();
}

bool MediaRenameDelegate::eventFilter(QObject *editor, QEvent *event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		switch (keyEvent->key()) {
		case Qt::Key_Escape: {
			QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
			if (lineEdit)
				lineEdit->undo();
			break;
		}
		case Qt::Key_Tab:
		case Qt::Key_Backtab:
			return false;
		}
	}

	return SoundTileDelegate::eventFilter(editor, event);
}

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("cg2121");
OBS_MODULE_USE_DEFAULT_LOCALE("Soundboard", "en-US")

bool obs_module_load(void)
{
	blog(LOG_INFO, "Soundboard plugin version %s is loaded", PLUGIN_VERSION);

	return true;
}

void obs_module_post_load(void)
{
	obs_frontend_push_ui_translation(obs_module_get_string);

	Soundboard *sb = new Soundboard();
	obs_frontend_add_dock_by_id("SoundboardDock", obs_module_text("Soundboard"), sb);

	obs_frontend_pop_ui_translation();
}

void obs_module_unload(void) {}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("Soundboard");
}
