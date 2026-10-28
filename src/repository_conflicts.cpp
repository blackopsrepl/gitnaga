#include "repository_controller.hpp"

#include "conflict_parser.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QStringDecoder>

#include <algorithm>

namespace GitNaga {

namespace {

const QStringList bisectVerdicts = { QStringLiteral("good"), QStringLiteral("bad"), QStringLiteral("skip"),
                                     QStringLiteral("reset") };

conflicts::Choice choiceFor(const QString &name)
{
    if (name == QStringLiteral("ours"))
        return conflicts::Choice::Ours;
    if (name == QStringLiteral("theirs"))
        return conflicts::Choice::Theirs;
    if (name == QStringLiteral("ours-theirs"))
        return conflicts::Choice::OursThenTheirs;
    if (name == QStringLiteral("theirs-ours"))
        return conflicts::Choice::TheirsThenOurs;
    return conflicts::Choice::Unresolved;
}

// The working file as text, or an error a person can act on: the built-in
// view only rewrites files it can round-trip exactly.
std::expected<QString, QString> readConflictFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::unexpected(QObject::tr("Cannot read %1: %2").arg(path, file.errorString()));
    const auto bytes = file.readAll();
    if (bytes.contains('\0'))
        return std::unexpected(QObject::tr("Binary file: use ours or theirs"));
    auto decoder = QStringDecoder(QStringDecoder::Utf8);
    QString text = decoder(bytes);
    if (decoder.hasError())
        return std::unexpected(QObject::tr("Not UTF-8 text: use ours, theirs or an editor"));
    return text;
}

} // namespace

void RepositoryController::continueOperation()
{
    const auto operation = m_repository.status.operation;
    if (operation.isEmpty() || operation == QStringLiteral("bisect"))
        return;
    const auto remaining = std::count_if(m_repository.status.files.cbegin(), m_repository.status.files.cend(),
                                         [](const FileChange &file) { return file.conflicted; });
    if (remaining > 0) {
        reportFailure(tr("Resolve %n conflicted file(s) first", nullptr, static_cast<int>(remaining)));
        return;
    }
    if (operation == QStringLiteral("merge")) {
        runUndoable(QStringLiteral("commit merge"), { QStringLiteral("commit"), QStringLiteral("--no-edit") },
                    tr("Merge committed"), undo::resetHead(tr("merge"), QStringLiteral("--keep")));
    } else if (operation == QStringLiteral("rebase") || operation == QStringLiteral("am")) {
        runUndoable(QStringLiteral("continue ") + operation, { operation, QStringLiteral("--continue") },
                    tr("Continued %1").arg(operation), undo::finishRebase(operation));
    } else {
        runUndoable(QStringLiteral("continue ") + operation, { operation, QStringLiteral("--continue") },
                    tr("Continued %1").arg(operation), undo::resetHead(operation, QStringLiteral("--keep")));
    }
}

void RepositoryController::skipOperation()
{
    const auto operation = m_repository.status.operation;
    if (!m_inProgress.canSkip())
        return;
    runOperation(QStringLiteral("skip ") + operation, { operation, QStringLiteral("--skip") },
                 tr("Skipped the current commit"));
}

void RepositoryController::abortOperation()
{
    const auto operation = m_repository.status.operation;
    if (operation.isEmpty())
        return;
    if (operation == QStringLiteral("bisect")) {
        bisect(QStringLiteral("reset"));
        return;
    }
    runOperation(QStringLiteral("abort ") + operation, { operation, QStringLiteral("--abort") },
                 tr("Aborted %1").arg(operation));
}

void RepositoryController::bisect(const QString &verdict)
{
    if (!bisectVerdicts.contains(verdict))
        return;
    runOperation(QStringLiteral("bisect ") + verdict, { QStringLiteral("bisect"), verdict },
                 verdict == QStringLiteral("reset") ? tr("Bisect ended") : tr("Marked %1").arg(verdict));
}

void RepositoryController::resolveConflict(const QString &path, const QString &side)
{
    if (path.isEmpty() || (side != QStringLiteral("ours") && side != QStringLiteral("theirs")))
        return;
    runAsyncOperation(tr("Resolved %1 with %2").arg(path, side),
                      [path, side](const QString &worktree, const RunOptions &options) -> GitResult<QString> {
        auto checkout = GitClient::mutate(worktree, { QStringLiteral("checkout"), QStringLiteral("--") + side,
                                                      QStringLiteral("--"), path },
                                          QStringLiteral("use ") + side, options);
        // A side that deleted the file has no version to check out: taking
        // that side means taking the deletion.
        if (!checkout && checkout.error().message.contains(QStringLiteral("does not have")))
            return GitClient::mutate(worktree, { QStringLiteral("rm"), QStringLiteral("--quiet"), QStringLiteral("--"), path },
                                     QStringLiteral("use ") + side, options);
        if (!checkout)
            return checkout;
        return GitClient::mutate(worktree, { QStringLiteral("add"), QStringLiteral("--"), path },
                                 QStringLiteral("mark resolved"), options);
    });
}

void RepositoryController::markResolved(const QString &path)
{
    if (path.isEmpty())
        return;
    runOperation(QStringLiteral("mark resolved"), { QStringLiteral("add"), QStringLiteral("-A"), QStringLiteral("--"), path },
                 tr("Marked %1 resolved").arg(path));
}

void RepositoryController::openInEditor(const QString &path) const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(m_repository.location.worktree).filePath(path)));
}

void RepositoryController::openMergetool(const QString &path) const
{
    // Detached: the tool is the person's own window and outlives nothing of ours.
    QProcess::startDetached(QStringLiteral("git"),
                            { QStringLiteral("mergetool"), QStringLiteral("--no-prompt"), QStringLiteral("--"), path },
                            m_repository.location.worktree);
}

QVariantMap RepositoryController::conflictDocument(const QString &path) const
{
    QVariantMap result;
    result.insert(QStringLiteral("path"), path);
    const auto text = readConflictFile(QDir(m_repository.location.worktree).filePath(path));
    if (!text) {
        result.insert(QStringLiteral("error"), text.error());
        return result;
    }
    const auto document = conflicts::parse(*text);
    QVariantList blocks;
    int ordinal = 0;
    for (const auto &block : document.blocks) {
        QVariantMap entry;
        entry.insert(QStringLiteral("conflict"), block.conflict);
        entry.insert(QStringLiteral("text"), block.text);
        entry.insert(QStringLiteral("ours"), block.ours);
        entry.insert(QStringLiteral("theirs"), block.theirs);
        entry.insert(QStringLiteral("base"), block.base);
        entry.insert(QStringLiteral("hasBase"), block.hasBase);
        entry.insert(QStringLiteral("oursLabel"), block.oursLabel);
        entry.insert(QStringLiteral("theirsLabel"), block.theirsLabel);
        entry.insert(QStringLiteral("index"), block.conflict ? ordinal++ : -1);
        blocks.append(entry);
    }
    result.insert(QStringLiteral("blocks"), blocks);
    result.insert(QStringLiteral("conflicts"), document.conflictCount());
    return result;
}

void RepositoryController::saveConflictResolution(const QString &path, const QStringList &choices)
{
    QVector<conflicts::Choice> picked;
    for (const auto &choice : choices)
        picked.append(choiceFor(choice));
    if (picked.contains(conflicts::Choice::Unresolved)) {
        reportFailure(tr("Choose a side for every conflict before saving"));
        return;
    }
    runAsyncOperation(tr("Saved the resolution of %1").arg(path),
                      [path, picked](const QString &worktree, const RunOptions &options) -> GitResult<QString> {
        const auto absolute = QDir(worktree).filePath(path);
        const auto text = readConflictFile(absolute);
        if (!text)
            return std::unexpected(GitError{ QStringLiteral("save resolution"), text.error() });
        const auto document = conflicts::parse(*text);
        // The file changed under the view (an editor saved it): its blocks
        // no longer line up with the choices made.
        if (document.conflictCount() != picked.size())
            return std::unexpected(GitError{ QStringLiteral("save resolution"),
                                             tr("%1 changed on disk; reopen it to resolve").arg(path) });
        QSaveFile file(absolute);
        if (!file.open(QIODevice::WriteOnly) || file.write(conflicts::resolve(document, picked).toUtf8()) < 0
            || !file.commit())
            return std::unexpected(GitError{ QStringLiteral("save resolution"), file.errorString() });
        return GitClient::mutate(worktree, { QStringLiteral("add"), QStringLiteral("--"), path },
                                 QStringLiteral("mark resolved"), options);
    });
}

} // namespace GitNaga
