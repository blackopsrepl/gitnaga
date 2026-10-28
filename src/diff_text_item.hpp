#pragma once

#include "diff_palette.hpp"

#include <QPointer>
#include <QQuickPaintedItem>
#include <QTextLayout>
#include <QVariant>

namespace GitNaga {

// One diff line, painted: syntax colours from the row's LineStyle and the
// intra-line changes as rounded marks behind the text. A painted item rather
// than a Text with markup, because a scrolled-in row then costs one cached
// QTextLayout instead of parsing rich text per delegate.
class DiffTextItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DiffText)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QVariant lineStyle READ lineStyle WRITE setLineStyle NOTIFY lineStyleChanged)
    Q_PROPERTY(bool added READ added WRITE setAdded NOTIFY addedChanged)
    Q_PROPERTY(GitNaga::DiffPalette *textPalette READ palette WRITE setPalette NOTIFY paletteChanged)
    Q_PROPERTY(bool wrap READ wrap WRITE setWrap NOTIFY wrapChanged)
    Q_PROPERTY(qreal offset READ offset WRITE setOffset NOTIFY offsetChanged)
    Q_PROPERTY(QColor colour READ colour WRITE setColour NOTIFY colourChanged)

public:
    explicit DiffTextItem(QQuickItem *parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString &text);
    QVariant lineStyle() const { return m_style; }
    void setLineStyle(const QVariant &style);
    bool added() const { return m_added; }
    void setAdded(bool added);
    DiffPalette *palette() const { return m_palette; }
    void setPalette(DiffPalette *palette);
    bool wrap() const { return m_wrap; }
    void setWrap(bool wrap);
    qreal offset() const { return m_offset; }
    void setOffset(qreal offset);
    // Overrides the plain-text colour (notices); transparent means unset.
    QColor colour() const { return m_colour; }
    void setColour(const QColor &colour);

    void paint(QPainter *painter) override;

signals:
    void textChanged();
    void lineStyleChanged();
    void addedChanged();
    void paletteChanged();
    void wrapChanged();
    void offsetChanged();
    void colourChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void invalidate();
    void ensureLayout();

    QString m_text;
    QVariant m_style;
    QPointer<DiffPalette> m_palette;
    QColor m_colour;
    QTextLayout m_layout;
    qreal m_offset = 0;
    qreal m_layoutWidth = -1;
    bool m_added = false;
    bool m_wrap = false;
    bool m_dirty = true;
};

} // namespace GitNaga
