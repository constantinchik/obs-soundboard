#pragma once

#include <QStyledItemDelegate>

class SceneTree;

/* Paints sounds as icon-centric tiles when the list is in grid mode:
 * the leading emoji of the name is drawn large, the rest of the name below it.
 * A playing tile is outlined; every running instance of the sound (clicking it
 * again stacks another one) sweeps its own progress fill and playhead line, newer
 * ones layered on top of older ones. */
class SoundTileDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	enum Role {
		ProgressRole = Qt::UserRole + 1, // QVariantList of double 0..1, oldest instance first
		PlayingRole = Qt::UserRole + 2,  // bool
		OverrideRole = Qt::UserRole + 3, // bool: tile overrides the default click/long-press action
	};

	explicit SoundTileDelegate(SceneTree *view);

	virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
			   const QModelIndex &index) const override;
	virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
					  const QModelIndex &index) const override;

	/* Splits "🚀LetsGo" into icon "🚀" and label "LetsGo". Returns false when the
	 * name does not start with an emoji; icon is then the first letter. */
	static bool splitName(const QString &name, QString &icon, QString &label);

private:
	SceneTree *view;
};
