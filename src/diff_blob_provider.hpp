#pragma once

#include <QQuickAsyncImageProvider>

namespace GitNaga {

// Serves image://diffblob/<key>/<generation>: one side of an image diff, read
// from a git object or the worktree on a pool thread, so a large PNG never
// blocks the window. `checker/…` is the transparency backdrop tile.
class DiffBlobProvider final : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};

} // namespace GitNaga
