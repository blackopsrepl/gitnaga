#include "operation_status.hpp"

#include "git_client.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QtConcurrentRun>

namespace GitNaga {

namespace {

QString firstLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readLine()).trimmed();
}

QString shortBranch(QString name)
{
    if (name.startsWith(QStringLiteral("refs/heads/")))
        name.remove(0, 11);
    return name;
}

// A commit is named by the branch that points at it when there is one, since
// "onto main" means more to a person than a hash.
QString nameFor(const QString &oid, const QVector<Ref> &refs)
{
    if (oid.isEmpty())
        return {};
    for (const auto kind : { RefKind::Local, RefKind::Remote, RefKind::Tag }) {
        for (const auto &ref : refs) {
            if (ref.kind == kind && !ref.internal && ref.target == oid)
                return ref.name;
        }
    }
    return oid.left(8);
}

QString mergedName(const QString &message)
{
    static const QRegularExpression pattern(
        QStringLiteral("^Merge (?:remote-tracking )?(?:branch|branches|commit|tag) '([^']+)'"));
    const auto match = pattern.match(message);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString progress(const OperationFacts &facts)
{
    return facts.total > 0 ? QStringLiteral(" — %1/%2").arg(facts.step).arg(facts.total) : QString();
}

QString targetLine(const OperationFacts &facts, const QString &verb)
{
    if (facts.target.isEmpty())
        return {};
    auto line = verb + QLatin1Char(' ') + facts.target.left(8);
    if (!facts.targetSubject.isEmpty())
        line += QStringLiteral(" · ") + facts.targetSubject;
    return line;
}

} // namespace

OperationFacts readOperationFacts(const RepositoryLocation &location, const QString &kind)
{
    OperationFacts facts;
    facts.kind = kind;
    const QDir git(location.gitDirectory);
    const auto read = [&git](const QString &name) { return firstLine(git.filePath(name)); };
    if (kind == QStringLiteral("rebase") || kind == QStringLiteral("am")) {
        const bool merge = QFileInfo(git.filePath(QStringLiteral("rebase-merge"))).isDir();
        const auto state = merge ? QStringLiteral("rebase-merge/") : QStringLiteral("rebase-apply/");
        facts.headName = shortBranch(read(state + QStringLiteral("head-name")));
        facts.onto = read(state + QStringLiteral("onto"));
        facts.step = read(state + (merge ? QStringLiteral("msgnum") : QStringLiteral("next"))).toInt();
        facts.total = read(state + (merge ? QStringLiteral("end") : QStringLiteral("last"))).toInt();
        facts.target = read(QStringLiteral("REBASE_HEAD"));
    } else if (kind == QStringLiteral("merge")) {
        facts.target = read(QStringLiteral("MERGE_HEAD"));
        facts.mergeMessage = read(QStringLiteral("MERGE_MSG"));
    } else if (kind == QStringLiteral("cherry-pick")) {
        facts.target = read(QStringLiteral("CHERRY_PICK_HEAD"));
    } else if (kind == QStringLiteral("revert")) {
        facts.target = read(QStringLiteral("REVERT_HEAD"));
    } else if (kind == QStringLiteral("bisect")) {
        facts.bisectStart = read(QStringLiteral("BISECT_START"));
        facts.target = read(QStringLiteral("HEAD"));
        if (facts.target.startsWith(QStringLiteral("ref: ")))
            facts.target.clear();
    }
    const auto worktree = location.worktree;
    if (!facts.target.isEmpty()) {
        RunOptions quiet;
        auto subject = GitClient::run(worktree, { QStringLiteral("log"), QStringLiteral("-1"),
                                                  QStringLiteral("--format=%s"), facts.target },
                                      QStringLiteral("read subject"), quiet);
        if (subject)
            facts.targetSubject = QString::fromUtf8(*subject).trimmed();
    }
    RunOptions optional;
    optional.acceptedExitCodes = { 0, 1 };
    auto tool = GitClient::run(worktree, { QStringLiteral("config"), QStringLiteral("--get"), QStringLiteral("merge.tool") },
                               QStringLiteral("read merge tool"), optional);
    facts.mergeTool = tool && !QString::fromUtf8(*tool).trimmed().isEmpty();
    return facts;
}

OperationText describeOperation(const OperationFacts &facts, const QString &currentBranch, const QVector<Ref> &refs)
{
    OperationText text;
    const auto branch = currentBranch.isEmpty() ? QObject::tr("HEAD") : currentBranch;
    const auto target = nameFor(facts.target, refs);
    if (facts.kind == QStringLiteral("rebase")) {
        const auto head = facts.headName.isEmpty() ? QObject::tr("HEAD") : facts.headName;
        const auto onto = nameFor(facts.onto, refs);
        text.title = QObject::tr("Rebasing %1 onto %2").arg(head, onto) + progress(facts);
        text.detail = targetLine(facts, QObject::tr("Stopped at"));
        text.ours = onto;
        text.theirs = facts.target.isEmpty() ? head : facts.target.left(8);
    } else if (facts.kind == QStringLiteral("am")) {
        text.title = QObject::tr("Applying patches onto %1").arg(facts.headName.isEmpty() ? branch : facts.headName)
                   + progress(facts);
        text.ours = branch;
        text.theirs = QObject::tr("patch");
    } else if (facts.kind == QStringLiteral("merge")) {
        auto merged = mergedName(facts.mergeMessage);
        if (merged.isEmpty())
            merged = target;
        text.title = QObject::tr("Merging %1 into %2").arg(merged, branch);
        text.detail = targetLine(facts, QObject::tr("Merge head"));
        text.ours = branch;
        text.theirs = merged;
    } else if (facts.kind == QStringLiteral("cherry-pick")) {
        text.title = QObject::tr("Cherry-picking %1 onto %2").arg(facts.target.left(8), branch);
        text.detail = facts.targetSubject;
        text.ours = branch;
        text.theirs = facts.target.left(8);
    } else if (facts.kind == QStringLiteral("revert")) {
        text.title = QObject::tr("Reverting %1 on %2").arg(facts.target.left(8), branch);
        text.detail = facts.targetSubject;
        text.ours = branch;
        text.theirs = QObject::tr("revert of %1").arg(facts.target.left(8));
    } else if (facts.kind == QStringLiteral("bisect")) {
        text.title = facts.bisectStart.isEmpty() ? QObject::tr("Bisecting")
                                                 : QObject::tr("Bisecting from %1").arg(shortBranch(facts.bisectStart));
        text.detail = targetLine(facts, QObject::tr("Testing"));
    }
    return text;
}

bool OperationStatus::canSkip() const
{
    return m_facts.kind == QStringLiteral("rebase") || m_facts.kind == QStringLiteral("cherry-pick")
        || m_facts.kind == QStringLiteral("am");
}

void OperationStatus::update(const RepositorySnapshot &repository)
{
    QStringList conflicts;
    for (const auto &file : repository.status.files) {
        if (file.conflicted)
            conflicts.append(file.path);
    }
    const auto kind = repository.status.operation;
    const auto generation = ++m_generation;
    if (kind.isEmpty() || repository.location.worktree.isEmpty()) {
        apply({}, conflicts, repository.refs);
        return;
    }
    auto *watcher = new QFutureWatcher<OperationFacts>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, generation, conflicts, refs = repository.refs] {
        watcher->deleteLater();
        if (generation == m_generation)
            apply(watcher->result(), conflicts, refs);
    });
    watcher->setFuture(QtConcurrent::run([location = repository.location, kind] {
        return readOperationFacts(location, kind);
    }));
}

void OperationStatus::apply(OperationFacts facts, QStringList conflicts, const RefSnapshot &refs)
{
    auto text = describeOperation(facts, refs.currentBranch, refs.refs);
    m_facts = std::move(facts);
    m_text = std::move(text);
    m_conflicts = std::move(conflicts);
    emit changed();
}

} // namespace GitNaga
