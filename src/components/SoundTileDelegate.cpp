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

/* Wraps the text at word boundaries into at most maxLines lines of the given width.
 * A word too long for a line, and whatever doesn't fit in the last line, gets "…". */
QStringList wrapAndElide(const QString &text, const QFont &font, qreal width, int maxLines)
{
	QStringList lines;
	if (text.isEmpty() || width <= 0 || maxLines < 1)
		return lines;

	QFontMetrics fm(font);
	QTextLayout layout(text, font);
	QTextOption opt;
	opt.setWrapMode(QTextOption::WordWrap);
	layout.setTextOption(opt);

	layout.beginLayout();

	for (;;) {
		QTextLine line = layout.createLine();
		if (!line.isValid())
			break;

		line.setLineWidth(width);

		QString lineText = (lines.size() == maxLines - 1) ? text.mid(line.textStart())
								  : text.mid(line.textStart(), line.textLength());
		lines << fm.elidedText(lineText.trimmed(), Qt::ElideRight, (int)width);

		if (lines.size() == maxLines)
			break;
	}

	layout.endLayout();
	return lines;
}

/* "StopTheCap.mp3" -> "StopTheCap": names made by dropping files keep the extension */
QString stripAudioExtension(const QString &label)
{
	static const char *exts[] = {".mp3", ".wav", ".ogg", ".flac", ".aac", ".m4a", ".opus"};

	for (const char *ext : exts) {
		if (label.endsWith(QLatin1String(ext), Qt::CaseInsensitive))
			return label.left(label.size() - (qsizetype)strlen(ext)).trimmed();
	}

	return label;
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
	const QVariantList progresses = index.data(ProgressRole).toList();

	QRectF tile = QRectF(option.rect).adjusted(tileMargin, tileMargin, -tileMargin, -tileMargin);
	QPainterPath shape;
	shape.addRoundedRect(tile, tileRadius, tileRadius);

	/* Background */
	QColor bg = textColor;
	bg.setAlphaF(hovered ? 0.13f : 0.06f);
	painter->fillPath(shape, bg);

	/* Playback progress: one translucent fill per running instance, oldest first,
	 * so each newer (shorter) sweep shows up darker on top of the older ones,
	 * and a playhead line at the edge of each, the newest one the strongest. */
	if (playing && !progresses.isEmpty()) {
		const qsizetype count = progresses.size();
		const float fillAlpha = count == 1 ? 0.40f : 0.28f;

		painter->save();
		painter->setClipPath(shape);

		for (qsizetype i = 0; i < count; i++) {
			const double progress = qBound(0.0, progresses[i].toDouble(), 1.0);
			const bool newest = i == count - 1;
			const qreal x = tile.left() + tile.width() * progress;

			QColor fillColor = accent;
			fillColor.setAlphaF(fillAlpha);
			painter->fillRect(QRectF(tile.left(), tile.top(), x - tile.left(), tile.height()), fillColor);

			if (count > 1 && progress > 0.0) {
				QColor line = accent;
				line.setAlphaF(newest ? 1.0f : 0.6f);
				painter->fillRect(QRectF(x - 1.0, tile.top(), 2.0, tile.height()), line);
			}
		}

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

	/* Subtle marker: this tile overrides the default click / long-press action */
	if (index.data(OverrideRole).toBool()) {
		QColor dot = textColor;
		dot.setAlphaF(0.45f);
		const qreal r = 2.5;
		painter->setPen(Qt::NoPen);
		painter->setBrush(dot);
		painter->drawEllipse(QPointF(tile.right() - 7.0, tile.top() + 7.0), r, r);
	}

	/* Icon and label */
	QString icon, label;
	const bool emoji = splitName(index.data(Qt::DisplayRole).toString(), icon, label);

	label = stripAudioExtension(label);

	/* Lay the label out first (one or two lines), then give the icon the rest */
	const qreal pad = 4.0;
	const QFont labelFont = option.font;
	const QFontMetricsF labelMetrics(labelFont);
	const qreal labelWidth = tile.width() - pad * 2.0;

	QStringList lines = wrapAndElide(label, labelFont, labelWidth, maxLabelLines);
	const qreal labelHeight = lines.size() * labelMetrics.lineSpacing();

	QRectF iconRect(tile.left(), tile.top() + pad, tile.width(), tile.height() - pad * 2.0 - labelHeight);
	QRectF labelRect(tile.left() + pad, iconRect.bottom(), labelWidth, labelHeight);

	QFont iconFont = option.font;
	iconFont.setPixelSize(qMax(12, (int)(qMin(iconRect.height(), 48.0) * (emoji ? 0.78 : 0.62))));
	iconFont.setBold(!emoji);

	painter->setFont(iconFont);
	painter->setPen(textColor);
	painter->drawText(iconRect, Qt::AlignCenter, icon);

	if (!lines.isEmpty()) {
		QColor labelColor = textColor;
		if (!playing && !selected)
			labelColor.setAlphaF(0.85f);

		painter->setFont(labelFont);
		painter->setPen(labelColor);
		painter->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, lines.join(QLatin1Char('\n')));
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
