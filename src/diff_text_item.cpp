#include "diff_text_item.hpp"

#include "diff_document.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QTextCharFormat>

#include <algorithm>
#include <cmath>

namespace GitNaga {

DiffPalette::DiffPalette(QObject *parent)
    : QObject(parent)
{
    m_font = QFont(QStringLiteral("monospace"));
    m_font.setStyleHint(QFont::Monospace);
    setPixelSize(12);
}

QColor DiffPalette::colour(int syntaxKind) const
{
    const QColor *colours[] = { &m_plain, &m_keyword, &m_type, &m_function, &m_string,
                                &m_number, &m_constant, &m_comment, &m_meta };
    const auto *colour = syntaxKind >= 0 && syntaxKind < 9 ? colours[syntaxKind] : &m_plain;
    return colour->isValid() ? *colour : m_plain;
}

void DiffPalette::setPixelSize(int size)
{
    if (size == m_font.pixelSize() || size <= 0)
        return;
    m_font.setPixelSize(size);
    const QFontMetricsF metrics(m_font);
    m_charWidth = metrics.horizontalAdvance(QLatin1Char('M'));
    // A little air between wrapped lines, matching the unwrapped row pitch.
    m_lineHeight = std::ceil(metrics.height()) + 3;
    emit changed();
}

DiffTextItem::DiffTextItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void DiffTextItem::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    invalidate();
    emit textChanged();
}

void DiffTextItem::setLineStyle(const QVariant &style)
{
    m_style = style;
    invalidate();
    emit lineStyleChanged();
}

void DiffTextItem::setAdded(bool added)
{
    if (added == m_added)
        return;
    m_added = added;
    update();
    emit addedChanged();
}

void DiffTextItem::setPalette(DiffPalette *palette)
{
    if (palette == m_palette)
        return;
    if (m_palette)
        disconnect(m_palette, nullptr, this, nullptr);
    m_palette = palette;
    if (m_palette)
        connect(m_palette, &DiffPalette::changed, this, &DiffTextItem::invalidate);
    invalidate();
    emit paletteChanged();
}

void DiffTextItem::setWrap(bool wrap)
{
    if (wrap == m_wrap)
        return;
    m_wrap = wrap;
    invalidate();
    emit wrapChanged();
}

void DiffTextItem::setOffset(qreal offset)
{
    if (qFuzzyCompare(offset, m_offset))
        return;
    m_offset = offset;
    if (!m_wrap)
        update();
    emit offsetChanged();
}

void DiffTextItem::setColour(const QColor &colour)
{
    if (colour == m_colour)
        return;
    m_colour = colour;
    update();
    emit colourChanged();
}

void DiffTextItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (m_wrap && !qFuzzyCompare(newGeometry.width(), oldGeometry.width()))
        invalidate();
}

void DiffTextItem::invalidate()
{
    m_dirty = true;
    // A wrapped line's height depends on its layout, and the list needs the
    // height before it paints; an unwrapped line is always one line tall.
    if (m_wrap)
        ensureLayout();
    else if (m_palette)
        setImplicitHeight(m_palette->lineHeight());
    update();
}

void DiffTextItem::ensureLayout()
{
    if (!m_palette || (!m_dirty && (!m_wrap || qFuzzyCompare(m_layoutWidth, width()))))
        return;
    m_dirty = false;
    m_layoutWidth = width();
    m_layout.clearLayout();
    m_layout.setText(m_text);
    m_layout.setFont(m_palette->font());
    QTextOption option;
    option.setWrapMode(m_wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    m_layout.setTextOption(option);

    QList<QTextLayout::FormatRange> formats;
    for (const auto &span : m_style.value<LineStyle>().syntax) {
        QTextCharFormat format;
        format.setForeground(m_palette->colour(static_cast<int>(span.kind)));
        if (span.kind == SyntaxKind::Comment)
            format.setFontItalic(true);
        formats.append({ span.start, span.length, format });
    }
    m_layout.setFormats(formats);

    const qreal pitch = m_palette->lineHeight();
    const qreal lead = (pitch - QFontMetricsF(m_palette->font()).height()) / 2;
    const qreal lineWidth = m_wrap && width() > 0 ? width() : 1e7;
    int lines = 0;
    m_layout.beginLayout();
    for (auto line = m_layout.createLine(); line.isValid(); line = m_layout.createLine()) {
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, lines * pitch + lead));
        ++lines;
    }
    m_layout.endLayout();
    setImplicitHeight(std::max(1, m_wrap && width() > 0 ? lines : 1) * pitch);
}

void DiffTextItem::paint(QPainter *painter)
{
    ensureLayout();
    if (!m_palette || m_text.isEmpty())
        return;
    const qreal pitch = m_palette->lineHeight();
    if (!m_wrap)
        painter->translate(-m_offset, std::floor((height() - pitch) / 2));
    const auto style = m_style.value<LineStyle>();
    if (!style.changed.isEmpty()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_palette->changedBackground(m_added));
        for (int index = 0; index < m_layout.lineCount(); ++index) {
            const auto line = m_layout.lineAt(index);
            const int lineStart = line.textStart();
            const int lineEnd = lineStart + line.textLength();
            for (const auto &range : style.changed) {
                const int start = std::max(range.start, lineStart);
                const int end = std::min(range.start + range.length, lineEnd);
                if (start >= end)
                    continue;
                const qreal left = line.cursorToX(start);
                const qreal right = line.cursorToX(end);
                painter->drawRoundedRect(QRectF(left - 1, index * pitch + 1, right - left + 2, pitch - 2), 3, 3);
            }
        }
    }
    painter->setPen(m_colour.alpha() > 0 ? m_colour : m_palette->plain());
    m_layout.draw(painter, QPointF(0, 0));
}

} // namespace GitNaga
