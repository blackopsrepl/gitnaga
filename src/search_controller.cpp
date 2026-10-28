#include "search_controller.hpp"

#include "repository_controller.hpp"

#include <QFutureWatcher>
#include <cstdio>
#include <QtConcurrentRun>

#include <algorithm>
#include <utility>

namespace GitNaga {

CommitSearch::CommitSearch(RepositoryController *repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
    // A pause in typing, not every keystroke, starts a search: a path or
    // content query is a git process, and a half-typed term is not a question.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(180);
    connect(&m_debounce, &QTimer::timeout, this, [this] { start(true); });
    m_remapTimer.setSingleShot(true);
    m_remapTimer.setInterval(150);
    connect(&m_remapTimer, &QTimer::timeout, this, &CommitSearch::historyMoved);

    if (!repository)
        return;
    auto *model = repository->commits();
    connect(model, &QAbstractItemModel::rowsInserted, this, [this, model](const QModelIndex &, int first, int last) {
        // Streamed chunks append and the uncommitted-work row comes and goes
        // on top; neither changes the commits already snapshotted.
        if (first != model->count() - (last - first + 1) && !isWorkInProgressRow(first, last))
            m_records.clear();
        // Copying each chunk as it lands spreads the cost over the load, so
        // the first search over a long history does not stall the window.
        snapshot();
        m_remapTimer.start();
    });
    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &, int first, int last) {
        if (!isWorkInProgressRow(first, last))
            m_records.clear();
    });
    connect(model, &QAbstractItemModel::rowsRemoved, &m_remapTimer, qOverload<>(&QTimer::start));
    connect(model, &QAbstractItemModel::modelReset, this, [this] {
        m_records.clear();
        m_historyReset = true;
        m_remapTimer.start();
    });
    connect(repository, &RepositoryController::repositoryChanged, this, &CommitSearch::clear);
    connect(repository, &RepositoryController::selectionChanged, this, &CommitSearch::positionChanged);
}

CommitSearch::~CommitSearch()
{
    if (m_cancel)
        m_cancel->store(true);
}

void CommitSearch::setQuery(const QString &query)
{
    if (query == m_query)
        return;
    m_query = query;
    emit queryChanged();
    m_debounce.start();
}

QString CommitSearch::kind() const
{
    return searchKindName(parseSearchQuery(m_query).kind);
}

int CommitSearch::position() const
{
    if (!m_repository)
        return 0;
    const auto it = std::ranges::lower_bound(m_rows, m_repository->selectedRow());
    return it != m_rows.cend() && *it == m_repository->selectedRow() ? static_cast<int>(it - m_rows.cbegin()) + 1 : 0;
}

bool CommitSearch::isMatch(int row) const
{
    if (m_hits.isEmpty() || !m_repository)
        return false;
    const auto *commit = m_repository->commits()->commitAt(row);
    return commit && m_hits.contains(commit->oid);
}

void CommitSearch::next()
{
    if (m_rows.isEmpty() || !m_repository)
        return;
    const auto it = std::ranges::upper_bound(m_rows, m_repository->selectedRow());
    selectRow(it == m_rows.cend() ? m_rows.first() : *it);
}

void CommitSearch::previous()
{
    if (m_rows.isEmpty() || !m_repository)
        return;
    const int selected = m_repository->selectedRow();
    const auto it = std::ranges::lower_bound(m_rows, selected < 0 ? 0 : selected);
    selectRow(it == m_rows.cbegin() ? m_rows.last() : *(it - 1));
}

void CommitSearch::clear()
{
    m_debounce.stop();
    m_query.clear();
    emit queryChanged();
    start(false);
}

bool CommitSearch::runNow()
{
    if (!m_debounce.isActive())
        return false;
    m_debounce.stop();
    start(true);
    return true;
}

void CommitSearch::selectRow(int row)
{
    if (const auto *commit = m_repository->commits()->commitAt(row))
        m_repository->selectOid(commit->oid);
}

bool CommitSearch::isWorkInProgressRow(int first, int last) const
{
    const auto *commit = m_repository->commits()->commitAt(first);
    return first == 0 && last == 0 && commit && commit->oid == worktreeOid();
}

QVector<SearchRecord> CommitSearch::snapshot()
{
    // Only rows streamed in since the last search are copied, so a search
    // over a long history costs the GUI thread nothing it already paid for;
    // the copies share their strings with the model, and the comparisons
    // themselves happen on the worker.
    const auto *model = m_repository->commits();
    const int offset = model->hasWorkInProgress() ? 1 : 0;
    for (int row = static_cast<int>(m_records.size()) + offset; row < model->count(); ++row) {
        const auto *commit = model->commitAt(row);
        if (commit && commit->oid != worktreeOid())
            m_records.append({ commit->oid, commit->subject, commit->author, commit->authorEmail });
    }
    return m_records;
}

QStringList CommitSearch::revisions() const
{
    // The same revisions the graph walks, so a hit is never a commit the
    // graph cannot show.
    HistoryRequest request;
    request.showInternal = m_repository->showInternalRefs();
    const auto hidden = m_repository->hiddenRefs();
    request.hidden = QSet<QString>(hidden.cbegin(), hidden.cend());
    request.solo = m_repository->soloRef();
    return GitClient::historyRevisions(m_repository->repository().refs, request);
}

void CommitSearch::start(bool fresh)
{
    if (m_cancel)
        m_cancel->store(true);
    const auto generation = ++m_generation;
    m_parsed = parseSearchQuery(m_query);
    if (m_parsed.isEmpty() || !m_repository || m_repository->repositoryPath().isEmpty()) {
        finish(generation, QStringList{}, false);
        return;
    }
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_clock.start();
    setRunning(true);
    auto *watcher = new QFutureWatcher<GitResult<QStringList>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, fresh] {
        watcher->deleteLater();
        finish(generation, watcher->result(), fresh);
    });
    const auto query = m_parsed;
    const auto cancel = m_cancel;
    if (query.needsGit()) {
        const auto worktree = m_repository->repositoryPath();
        const auto revisions = this->revisions();
        watcher->setFuture(QtConcurrent::run([worktree, revisions, query, cancel] {
            return searchHistory(worktree, revisions, query, cancel);
        }));
    } else {
        QElapsedTimer probe; probe.start(); auto records = snapshot(); fprintf(stderr, "SNAPSHOT %lld ms %lld\n", (long long)probe.elapsed(), (long long)records.size());
        watcher->setFuture(QtConcurrent::run([records, query, cancel]() -> GitResult<QStringList> {
            return matchRecords(records, query, cancel);
        }));
    }
}

void CommitSearch::finish(quint64 generation, const GitResult<QStringList> &result, bool fresh)
{
    if (generation != m_generation)
        return;
    setRunning(false);
    m_elapsedMs = m_clock.isValid() ? static_cast<int>(m_clock.elapsed()) : 0;
    m_error = result ? QString() : result.error().message;
    m_hits = result ? QSet<QString>(result->cbegin(), result->cend()) : QSet<QString>{};
    remap();
    // A new question moves to its first answer at or after the selection; a
    // rerun after the history moved leaves the selection where it is.
    if (fresh && !m_rows.isEmpty() && position() == 0)
        next();
}

void CommitSearch::remap()
{
    m_rows.clear();
    if (m_repository) {
        const auto *model = m_repository->commits();
        for (const auto &oid : std::as_const(m_hits)) {
            if (const int row = model->rowOf(oid); row >= 0)
                m_rows.append(row);
        }
    }
    std::ranges::sort(m_rows);
    ++m_revision;
    emit resultsChanged();
    emit positionChanged();
}

void CommitSearch::historyMoved()
{
    if (m_parsed.isEmpty())
        return;
    // Loaded commits may now answer a text query that they did not before.
    // Git's answer to a path or content query only needs re-placing as rows
    // stream in; it is asked again only when the history was read anew.
    const bool reset = std::exchange(m_historyReset, false);
    if (!m_parsed.needsGit() || reset)
        start(false);
    else if (!m_running)
        remap();
}

void CommitSearch::setRunning(bool running)
{
    if (running == m_running)
        return;
    m_running = running;
    emit runningChanged();
}

} // namespace GitNaga
