#include "MediaEdit.hpp"
#include "ui_MediaEdit.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include "models/MediaData.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>

#include "moc_MediaEdit.cpp"

#define QTStr(str) QString(obs_module_text(str))
#define MainStr(str) QString(obs_frontend_get_locale_string(str))

MediaEdit::MediaEdit(QWidget *parent) : QDialog(parent), ui(new Ui_MediaEdit)
{
	obs_frontend_push_ui_translation(obs_module_get_string);
	ui->setupUi(this);
	obs_frontend_pop_ui_translation();

	connect(ui->name, &QLineEdit::textEdited, this, [this]() { nameTouched = true; });
	connect(ui->name, &QLineEdit::textChanged, this, &MediaEdit::updateFileState);
	connect(ui->path, &QLineEdit::textChanged, this, &MediaEdit::updateFileState);

	ui->browseButton->setFocus();
	updateFileState();
}

void MediaEdit::updateFileState()
{
	// Nothing to name or save until a file is chosen
	const bool hasFile = !ui->path->text().isEmpty();
	ui->name->setEnabled(hasFile);

	QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok);
	if (ok)
		ok->setEnabled(hasFile && !ui->name->text().trimmed().isEmpty());
}

QString MediaEdit::uniqueName(const QString &base)
{
	if (base == origText || !MediaObj::findByName(base))
		return base;

	for (int i = 2;; i++) {
		QString candidate = base + " " + QString::number(i);
		if (!MediaObj::findByName(candidate))
			return candidate;
	}
}

MediaEdit::~MediaEdit() {}

void MediaEdit::setName(const QString &name)
{
	ui->name->setText(name);

	if (origText.isNull())
		origText = name;
}

QString MediaEdit::getName()
{
	return ui->name->text();
}

void MediaEdit::setPath(const QString &path)
{
	ui->path->setText(path);
}

QString MediaEdit::getPath()
{
	return ui->path->text();
}

void MediaEdit::setLoopChecked(bool checked)
{
	ui->loop->setChecked(checked);
}

bool MediaEdit::loopChecked()
{
	return ui->loop->isChecked();
}

QString MediaEdit::actionName(int action)
{
	switch (action) {
	case SoundActionRestart:
		return QTStr("Action.Restart");
	case SoundActionStop:
		return QTStr("Action.Stop");
	case SoundActionPlayAgain:
	default:
		return QTStr("Action.PlayAgain");
	}
}

static void fillActionCombo(QComboBox *combo, int defaultAction)
{
	combo->clear();
	combo->addItem(QTStr("Action.Default").arg(MediaEdit::actionName(defaultAction)), (int)SoundActionDefault);
	combo->addItem(MediaEdit::actionName(SoundActionPlayAgain), (int)SoundActionPlayAgain);
	combo->addItem(MediaEdit::actionName(SoundActionRestart), (int)SoundActionRestart);
	combo->addItem(MediaEdit::actionName(SoundActionStop), (int)SoundActionStop);
}

static void selectAction(QComboBox *combo, int action)
{
	int index = combo->findData(action);
	combo->setCurrentIndex(index >= 0 ? index : 0);
}

void MediaEdit::setDefaultActions(int defaultClick, int defaultLongPress)
{
	fillActionCombo(ui->clickAction, defaultClick);
	fillActionCombo(ui->longPressAction, defaultLongPress);
}

void MediaEdit::setClickAction(int action)
{
	selectAction(ui->clickAction, action);
}

int MediaEdit::clickAction()
{
	QVariant data = ui->clickAction->currentData();
	return data.isValid() ? data.toInt() : (int)SoundActionDefault;
}

void MediaEdit::setLongPressAction(int action)
{
	selectAction(ui->longPressAction, action);
}

int MediaEdit::longPressAction()
{
	QVariant data = ui->longPressAction->currentData();
	return data.isValid() ? data.toInt() : (int)SoundActionDefault;
}

void MediaEdit::on_browseButton_clicked()
{
	QString folder = ui->path->text();

	if (!folder.isEmpty())
		folder = QFileInfo(folder).absoluteDir().absolutePath();
	else
		folder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

	QString fileName = QFileDialog::getOpenFileName(this, QTStr("OpenAudioFile"), folder,
							("Audio (*.mp3 *.aac *.ogg *.wav *.flac)"));

	if (fileName.isEmpty())
		return;

	ui->path->setText(fileName);

	// A new sound takes its name from the file until the user types one;
	// editing an existing sound keeps its name
	if (origText.isEmpty() && !nameTouched)
		ui->name->setText(uniqueName(QFileInfo(fileName).completeBaseName()));
}

void MediaEdit::on_buttonBox_clicked(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole val = ui->buttonBox->buttonRole(button);

	if (val == QDialogButtonBox::RejectRole) {
		close();
	} else if (val == QDialogButtonBox::AcceptRole) {
		QString name = getName();

		if (name.isEmpty()) {
			QMessageBox::warning(this, MainStr("EmptyName.Title"), MainStr("EmptyName.Text"));
			return;
		}

		if (origText != name && MediaObj::findByName(name)) {
			QMessageBox::warning(this, MainStr("NameExists.Title"), MainStr("NameExists.Text"));
			return;
		}

		if (ui->path->text().isEmpty()) {
			QMessageBox::warning(this, QTStr("NoFile.Title"), QTStr("NoFile.Text"));
			return;
		}

		accept();
		close();
	}
}
