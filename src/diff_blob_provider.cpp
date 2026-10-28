#include "diff_blob_provider.hpp"

#include "git_client.hpp"

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QRunnable>
#include <QThreadPool>

namespace GitNaga {

namespace {

// checker/<dark>/<light>: the theme's two colours, as hex without '#'.
QImage checkerTile(const QString &id)
{
    QImage tile(16, 16, QImage::Format_RGB32);
    tile.fill(QColor(QLatin1Char('#') + id.section(QLatin1Char('/'), 1, 1)));
    QPainter painter(&tile);
    const QColor light(QLatin1Char('#') + id.section(QLatin1Char('/'), 2, 2));
    painter.fillRect(0, 0, 8, 8, light);
    painter.fillRect(8, 8, 8, 8, light);
    return tile;
}

QImage decodeBlob(const QString &id, const QSize &requestedSize, QString &error)
{
    const auto key = QByteArray::fromBase64(id.section(QLatin1Char('/'), 0, 0).toLatin1(),
                                            QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    const auto parts = QString::fromUtf8(key).split(QLatin1Char('\n'));
    if (parts.size() != 3) {
        error = QStringLiteral("Malformed image id");
        return {};
    }
    const BlobSource source{ parts.at(1) == QStringLiteral("file") ? BlobSource::Kind::File : BlobSource::Kind::Object,
                             parts.at(2) };
    auto bytes = GitClient::readBlob(parts.at(0), source);
    if (!bytes) {
        error = bytes.error().message;
        return {};
    }
    QBuffer buffer(&*bytes);
    buffer.open(QIODevice::ReadOnly);
    // The suffix is a hint only; the reader still sniffs the content.
    QImageReader reader(&buffer, QFileInfo(source.spec).suffix().toLower().toLatin1());
    reader.setAutoTransform(true);
    if (requestedSize.isValid() && reader.size().isValid()) {
        const auto natural = reader.size();
        if (natural.width() > requestedSize.width() || natural.height() > requestedSize.height())
            reader.setScaledSize(natural.scaled(requestedSize, Qt::KeepAspectRatio));
    }
    auto image = reader.read();
    if (image.isNull())
        error = reader.errorString();
    return image;
}

class BlobResponse final : public QQuickImageResponse, public QRunnable
{
public:
    BlobResponse(QString id, QSize requestedSize)
        : m_id(std::move(id))
        , m_requestedSize(requestedSize)
    {
        setAutoDelete(false);
    }

    void run() override
    {
        m_image = m_id.startsWith(QStringLiteral("checker/")) ? checkerTile(m_id) : decodeBlob(m_id, m_requestedSize, m_error);
        emit finished();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }
    QString errorString() const override { return m_error; }

private:
    QString m_id;
    QSize m_requestedSize;
    QImage m_image;
    QString m_error;
};

} // namespace

QQuickImageResponse *DiffBlobProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    auto *response = new BlobResponse(id, requestedSize);
    QThreadPool::globalInstance()->start(response);
    return response;
}

} // namespace GitNaga
