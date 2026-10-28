#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace GitNaga {

// The diff's colours and its one monospace font, set once from NagaTheme and
// shared by every painted line, so a scrolled-in row binds one pointer
// instead of a dozen colours.
class DiffPalette : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QColor plain MEMBER m_plain NOTIFY changed)
    Q_PROPERTY(QColor keyword MEMBER m_keyword NOTIFY changed)
    Q_PROPERTY(QColor type MEMBER m_type NOTIFY changed)
    Q_PROPERTY(QColor functionCall MEMBER m_function NOTIFY changed)
    Q_PROPERTY(QColor string MEMBER m_string NOTIFY changed)
    Q_PROPERTY(QColor number MEMBER m_number NOTIFY changed)
    Q_PROPERTY(QColor constant MEMBER m_constant NOTIFY changed)
    Q_PROPERTY(QColor comment MEMBER m_comment NOTIFY changed)
    Q_PROPERTY(QColor meta MEMBER m_meta NOTIFY changed)
    Q_PROPERTY(QColor addedWord MEMBER m_addedWord NOTIFY changed)
    Q_PROPERTY(QColor removedWord MEMBER m_removedWord NOTIFY changed)
    Q_PROPERTY(int pixelSize READ pixelSize WRITE setPixelSize NOTIFY changed)
    Q_PROPERTY(qreal charWidth READ charWidth NOTIFY changed)
    Q_PROPERTY(qreal lineHeight READ lineHeight NOTIFY changed)

public:
    explicit DiffPalette(QObject *parent = nullptr);

    QColor colour(int syntaxKind) const;
    QColor plain() const { return m_plain; }
    QColor changedBackground(bool added) const { return added ? m_addedWord : m_removedWord; }
    const QFont &font() const { return m_font; }
    int pixelSize() const { return m_font.pixelSize(); }
    void setPixelSize(int size);
    qreal charWidth() const { return m_charWidth; }
    qreal lineHeight() const { return m_lineHeight; }

signals:
    void changed();

private:
    QColor m_plain{ 0xcf, 0xd5, 0xe2 };
    QColor m_keyword, m_type, m_function, m_string, m_number, m_constant, m_comment, m_meta;
    QColor m_addedWord, m_removedWord;
    QFont m_font;
    qreal m_charWidth = 7;
    qreal m_lineHeight = 16;
};

} // namespace GitNaga
