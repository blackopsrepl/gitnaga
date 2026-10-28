#include "navigation_controller.hpp"

#include "recent_projects.hpp"
#include "repository_controller.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace GitNaga {

NavigationController::NavigationController(RepositoryController *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_search(repository, this)
    , m_fileHistory(this)
    , m_blame(this)
{
}

void NavigationController::showFileHistory(const QString &path, const QString &revision)
{
    if (!m_repository || path.isEmpty())
        return;
    // The worktree's history of a file is HEAD's: uncommitted work has none.
    const auto start = revision == worktreeOid() ? QString() : revision;
    m_fileHistory.load(m_repository->repositoryPath(), start, path);
    emit fileHistoryRequested();
}

void NavigationController::showBlame(const QString &path, const QString &revision)
{
    if (!m_repository || path.isEmpty())
        return;
    auto at = revision == worktreeOid() ? QString() : revision;
    // A file deleted by the reviewed commit has no lines there; what a person
    // wants is who wrote the lines that commit removed.
    const auto *files = m_repository->changedFiles();
    for (int row = 0; row < files->rowCount() && !at.isEmpty(); ++row) {
        const auto *file = files->fileAt(row);
        if (file->path == path && file->status == QStringLiteral("D") && at == m_repository->selectedOid())
            at = m_repository->selectedParents().value(0, at);
    }
    m_blame.load(m_repository->repositoryPath(), at, path);
    emit blameRequested();
}

QString NavigationController::selectedPath() const
{
    if (!m_repository)
        return {};
    const auto *file = m_repository->changedFiles()->fileAt(m_repository->selectedFile());
    return file ? file->path : QString();
}

int NavigationController::parentRow(int row) const
{
    if (!m_repository)
        return -1;
    const auto *model = m_repository->commits();
    const auto *commit = model->commitAt(row);
    return commit && !commit->parents.isEmpty() ? model->rowOf(commit->parents.first()) : -1;
}

int NavigationController::childRow(int row) const
{
    if (!m_repository)
        return -1;
    // Topological order puts every child above its parent; the nearest one
    // is the step a person means by "forward".
    const auto *model = m_repository->commits();
    const auto *commit = model->commitAt(row);
    if (!commit)
        return -1;
    for (int candidate = row - 1; candidate >= 0; --candidate) {
        if (model->commitAt(candidate)->parents.contains(commit->oid))
            return candidate;
    }
    return -1;
}

QString NavigationController::resolveCommit(const QString &prefix) const
{
    static const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]{4,40}$"));
    if (!m_repository || !hex.match(prefix).hasMatch())
        return {};
    const auto needle = prefix.toLower();
    const auto *model = m_repository->commits();
    if (const int row = model->rowOf(needle); row >= 0)
        return needle;
    QString found;
    for (int row = 0; row < model->count(); ++row) {
        const auto &oid = model->commitAt(row)->oid;
        if (!oid.startsWith(needle))
            continue;
        if (!found.isEmpty())
            return {};
        found = oid;
    }
    return found;
}

int NavigationController::commandScore(const QString &needle, const QString &label)
{
    const auto score = RecentProjects::fuzzyScore(needle, label);
    if (score < 0 || needle.isEmpty())
        return score;
    // A literal word beats letters scattered across the label: "push" should
    // rank "Push" above "Previous commit, show history".
    const auto at = label.indexOf(needle, 0, Qt::CaseInsensitive);
    if (at == 0)
        return score + 60;
    if (at > 0)
        return score + (label.at(at - 1).isLetterOrNumber() ? 20 : 40);
    return score;
}

QVariantList NavigationController::rankCommands(const QStringList &labels, const QString &needle) const
{
    QVector<QPair<int, int>> scored;
    const auto trimmed = needle.trimmed();
    for (qsizetype index = 0; index < labels.size(); ++index) {
        const int score = commandScore(trimmed, labels.at(index));
        if (score >= 0)
            scored.append({ score, static_cast<int>(index) });
    }
    std::ranges::stable_sort(scored, [](const auto &a, const auto &b) { return a.first > b.first; });
    QVariantList order;
    for (const auto &entry : scored)
        order.append(entry.second);
    return order;
}

} // namespace GitNaga
