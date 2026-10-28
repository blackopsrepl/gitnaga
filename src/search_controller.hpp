#pragma once

#include "commit_search.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

namespace GitNaga {

class RepositoryController;

// The graph's search: what was asked, which loaded commits answer it, and
// which of them is current. Matching runs off the GUI thread; hits are kept
// by oid, so the uncommitted-work row coming and going or a reload moving
// rows never points a highlight at the wrong commit.
class CommitSearch final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY queryChanged)
    Q_PROPERTY(bool active READ active NOTIFY resultsChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int count READ count NOTIFY resultsChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    // Bumped whenever the hit set changes, so per-row bindings re-evaluate.
    Q_PROPERTY(int revision READ revision NOTIFY resultsChanged)
    Q_PROPERTY(QString error READ error NOTIFY resultsChanged)
    Q_PROPERTY(int elapsedMs READ elapsedMs NOTIFY resultsChanged)

public:
    explicit CommitSearch(RepositoryController *repository, QObject *parent = nullptr);
    ~CommitSearch() override;

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    QString kind() const;
    bool active() const { return !m_parsed.isEmpty(); }
    bool running() const { return m_running; }
    int count() const { return static_cast<int>(m_rows.size()); }
    int position() const;
    int revision() const { return m_revision; }
    QString error() const { return m_error; }
    int elapsedMs() const { return m_elapsedMs; }
    const QVector<int> &rows() const { return m_rows; }

    Q_INVOKABLE bool isMatch(int row) const;
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void clear();
    // Starts the pending query now instead of after the typing pause.
    Q_INVOKABLE void runNow();

signals:
    void queryChanged();
    void resultsChanged();
    void runningChanged();
    void positionChanged();

private:
    void start(bool fresh);
    void finish(quint64 generation, const GitResult<QStringList> &result, bool fresh);
    void remap();
    void historyMoved();
    void setRunning(bool running);
    void selectRow(int row);
    QVector<SearchRecord> snapshot() const;
    QStringList revisions() const;

    QPointer<RepositoryController> m_repository;
    QString m_query;
    SearchQuery m_parsed;
    QSet<QString> m_hits;
    QVector<int> m_rows;
    QString m_error;
    QTimer m_debounce;
    QTimer m_remapTimer;
    QElapsedTimer m_clock;
    CancelToken m_cancel;
    quint64 m_generation = 0;
    int m_revision = 0;
    int m_elapsedMs = 0;
    bool m_running = false;
    bool m_historyReset = false;
};

} // namespace GitNaga
