#include "git_client.hpp"

#include "git_parse.hpp"

#include <QDateTime>

namespace GitNaga {

using detail::decode;

QStringList GitClient::historyRevisions(const RefSnapshot &refs, const HistoryRequest &request)
{
    QStringList revisions;
    if (!request.solo.isEmpty()) {
        revisions.append(request.solo);
    } else {
        for (const auto &ref : refs.refs) {
            if (ref.target.isEmpty() || ref.kind == RefKind::Stash)
                continue;
            if (ref.internal && !request.showInternal)
                continue;
            if (request.hidden.contains(ref.fullName))
                continue;
            revisions.append(ref.fullName);
        }
    }
    // HEAD always: a detached checkout has no ref of its own, and the line the
    // user is working on is never hidden from them.
    if (!refs.headOid.isEmpty())
        revisions.append(QStringLiteral("HEAD"));
    return revisions;
}

GitResult<qsizetype> GitClient::streamHistory(const QString &worktree, const QStringList &revisions,
                                              const QHash<QString, QVector<Ref>> &decorations, int chunkSize,
                                              const std::function<void(QVector<Commit> &&chunk)> &onChunk,
                                              CancelToken cancel)
{
    if (revisions.isEmpty())
        return 0;

    detail::GraphLayout layout;
    // Authors repeat on almost every row; sharing one string per author keeps a
    // long history's memory in proportion to its people, not its commits.
    QHash<QByteArray, QString> interned;
    const auto intern = [&interned](const QByteArray &bytes) {
        auto it = interned.constFind(bytes);
        if (it != interned.constEnd())
            return *it;
        return *interned.insert(bytes, decode(bytes));
    };

    QVector<Commit> chunk;
    qsizetype total = 0;
    // The first chunk is small so the first screen paints at once.
    int limit = std::min(chunkSize, 300);
    QByteArray pending;

    const auto flush = [&] {
        if (chunk.isEmpty())
            return;
        total += chunk.size();
        onChunk(std::move(chunk));
        chunk = {};
        limit = chunkSize;
    };

    const auto consume = [&](const QByteArray &bytes) {
        pending.append(bytes);
        qsizetype start = 0;
        for (;;) {
            const auto end = pending.indexOf('\x1e', start);
            if (end < 0)
                break;
            auto record = QByteArray::fromRawData(pending.constData() + start, end - start);
            while (!record.isEmpty() && (record.front() == '\n'))
                record = record.mid(1);
            start = end + 1;
            const auto parts = record.split('\x1f');
            if (parts.size() < 6)
                continue;
            Commit commit;
            commit.oid = decode(parts.at(0));
            if (!parts.at(1).isEmpty())
                commit.parents = decode(parts.at(1)).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            commit.author = intern(parts.at(2));
            commit.authorEmail = intern(parts.at(3).trimmed().toLower());
            commit.authoredAt = QDateTime::fromSecsSinceEpoch(parts.at(4).toLongLong());
            commit.subject = decode(parts.at(5));
            commit.refs = decorations.value(commit.oid);
            layout.place(commit);
            chunk.append(std::move(commit));
            if (chunk.size() >= limit)
                flush();
        }
        pending.remove(0, start);
        return true;
    };

    RunOptions options;
    options.cancel = std::move(cancel);
    options.input = revisions.join(QLatin1Char('\n')).toUtf8() + '\n';
    options.output = consume;
    auto result = run(worktree,
                      { QStringLiteral("log"), QStringLiteral("--stdin"), QStringLiteral("--topo-order"),
                        QStringLiteral("--date-order"), QStringLiteral("--no-color"),
                        QStringLiteral("--format=%H%x1f%P%x1f%an%x1f%ae%x1f%at%x1f%s%x1e") },
                      QStringLiteral("load history"), options);
    if (!result)
        return std::unexpected(result.error());
    flush();
    return total;
}

} // namespace GitNaga
