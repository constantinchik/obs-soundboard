#include "SoundTileDelegate.hpp"
#include "SceneTree.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QTextBoundaryFinder>
#include <QTextLayout>

#include "moc_SoundTileDelegate.cpp"

namespace {

constexpr qreal tileMargin = 3.0;
constexpr qreal tileRadius = 6.0;
constexpr int maxLabelLines = 2;

/* Lays the text out in at most maxLines lines of the given width, eliding the last one. */
QString wrapAndElide(const QString &text, const QFont &font, qreal width, int maxLines)
{
	if (text.isEmpty() || width <= 0)
		return QString();

	QFontMetrics fm(font);
	QTextLayout layout(text, font);
	QTextOption opt;
	opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	layout.setTextOption(opt);

	QStringList lines;
	layout.beginLayout();

	for (;;) {
		QTextLine line = layout.createLine();
		if (!line.isValid())
			break;

		line.setLineWidth(width);

		if (lines.size() == maxLines - 1) {
			QString rest = text.mid(line.textStart()).trimmed();
			lines << fm.elidedText(rest, Qt::ElideRight, (int)width);
			break;
		}

		lines << text.mid(line.textStart(), line.textLength()).trimmed();
	}

	layout.endLayout();
	return lines.join(QLatin1Char('\n'));
}

} // namespace

SoundTileDelegate::SoundTileDelegate(SceneTree *view_) : QStyledItemDelegate(view_), view(view_) {}

bool SoundTileDelegate::splitName(const QString &name, QString &icon, QString &label)
{
	QString trimmed = name.trimmed();
	icon.clear();
	label = trimmed;

	if (trimmed.isEmpty())
		return false;

	QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, trimmed);
	qsizetype end = finder.toNextBoundary();
	if (end <= 0)
		end = 1;

	QString first = trimmed.left(end);
	QList<uint> ucs4 = first.toUcs4();
	uint cp = ucs4.isEmpty() ? 0 : ucs4.first();

	bool emoji = cp >= 0x2000 && QChar::category(char32_t(cp)) == QChar::Symbol_Other;

	if (emoji) {
		icon = first;
		label = trimmed.mid(end).trimmed();
	} else {
		icon = first.toUpper();
	}

	return emoji;
}

void SoundTileDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	if (!view || !view->GetGridMode()) {
		QStyledItemDelegate::paint(painter, option, index);
		return;
	}

	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setRenderHint(QPainter::TextAntialiasing, true);

	const QPalette &pal = option.palette;
	const QColor textColor = pal.color(QPalette::Text);
	const QColor accent = pal.color(QPalette::Highlight);

	const bool selected = option.state.testFlag(QStyle::State_Selected);
	const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
	const bool playing = index.data(PlayingRole).toBool();
	const double progress = qBound(0.0, index.data(ProgressRole).toDouble(), 1.0);

	QRectF tile = QRectF(option.rect).adjusted(tileMargin, tileMargin, -tileMargin, -tileMargin);
	QPainterPath shape;
	shape.addRoundedRect(tile, tileRadius, tileRadius);

	/* Background */
	QColor bg = textColor;
	bg.setAlphaF(hovered ? 0.13f : 0.06f);
	painter->fillPath(shape, bg);

	/* Playback progress */
	if (playing && progress > 0.0) {
		QRectF fill = tile;
		fill.setWidth(tile.width() * progress);
		QColor fillColor = accent;
		fillColor.setAlphaF(0.40f);

		painter->save();
		painter->setClipPath(shape);
		painter->fillRect(fill, fillColor);
		painter->restore();
	}

	/* Outline: solid accent while playing, faint accent when only selected */
	if (playing || selected) {
		QColor border = accent;
		qreal penWidth = 2.0;

		if (!playing) {
			border.setAlphaF(0.55f);
			penWidth = 1.0;
		}

		qreal inset = penWidth / 2.0;
		painter->setPen(QPen(border, penWidth));
		painter->setBrush(Qt::NoBrush);
		painter->drawRoundedRect(tile.adjusted(inset, inset, -inset, -inset), tileRadius, tileRadius);
	}

	/* Icon and label */
	QString icon, label;
	const bool emoji = splitName(index.data(Qt::DisplayRole).toString(), icon, label);

	const qreal pad = 4.0;
	QRectF iconRect(tile.left(), tile.top() + pad, tile.width(), tile.height() * 0.55 - pad);
	QRectF labelRect(tile.left() + pad, iconRect.bottom() + 2.0, tile.width() - pad * 2.0,
			 tile.bottom() - iconRect.bottom() - 2.0 - pad);

	if (label.isEmpty()) {
		/* Emoji-only name: let the icon use the whole tile */
		iconRect = tile.adjusted(0, pad, 0, -pad);
	}

	QFont iconFont = option.font;
	iconFont.setPixelSize(qMax(12, (int)(qMin(iconRect.height(), 48.0) * (emoji ? 0.78 : 0.62))));
	iconFont.setBold(!emoji);

	painter->setFont(iconFont);
	painter->setPen(textColor);
	painter->drawText(iconRect, Qt::AlignCenter, icon);

	if (!label.isEmpty()) {
		QFont labelFont = option.font;
		QString text = wrapAndElide(label, labelFont, labelRect.width(), maxLabelLines);

		QColor labelColor = textColor;
		if (!playing && !selected)
			labelColor.setAlphaF(0.85f);

		painter->setFont(labelFont);
		painter->setPen(labelColor);
		painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, text);
	}

	painter->restore();
}

void SoundTileDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
					     const QModelIndex &index) const
{
	if (!view || !view->GetGridMode()) {
		QStyledItemDelegate::updateEditorGeometry(editor, option, index);
		return;
	}

	QRect r = option.rect.adjusted(6, 0, -6, -6);
	int h = editor->sizeHint().height();
	editor->setGeometry(r.left(), r.bottom() - h, r.width(), h);
}
