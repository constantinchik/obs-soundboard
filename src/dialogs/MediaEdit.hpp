#pragma once

#include <QDialog>
#include <memory>

class QAbstractButton;
class Ui_MediaEdit;

class MediaEdit : public QDialog {
	Q_OBJECT

private:
	QString origText;
	std::unique_ptr<Ui_MediaEdit> ui;

private slots:
	void on_browseButton_clicked();
	void on_buttonBox_clicked(QAbstractButton *button);

public:
	MediaEdit(QWidget *parent = nullptr);
	~MediaEdit();

	void setName(const QString &name);
	QString getName();

	void setPath(const QString &path);
	QString getPath();

	void setLoopChecked(bool checked);
	bool loopChecked();

	// Fills both action combos; the "Default" entry names the current global default.
	void setDefaultActions(int defaultClick, int defaultLongPress);

	void setClickAction(int action);
	int clickAction();

	void setLongPressAction(int action);
	int longPressAction();

	static QString actionName(int action);
};
