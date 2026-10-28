#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace GitNaga {

class RepositoryController;

// The repository's conversation with its remotes, as `repository.sync` in QML:
// fetch (by hand and every ten minutes while the window is in use), pull in
// the remembered mode, push with publishing and a lease-guarded force, and
// upstream tracking. Everything runs through the controller's operation queue.
class RemoteSync final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autoFetch READ autoFetch WRITE setAutoFetch NOTIFY settingsChanged)
    Q_PROPERTY(QString pullMode READ pullMode WRITE setPullMode NOTIFY settingsChanged)

public:
    explicit RemoteSync(RepositoryController *repository);

    bool autoFetch() const;
    void setAutoFetch(bool enabled);
    QString pullMode() const;
    void setPullMode(const QString &mode);

    // The program git and ssh run to ask for a password: this binary.
    static void setAskpassProgram(const QString &program);
    static QString askpassProgram();
    // Tests shorten the interval and stand in for the window being active.
    void setAutoFetchInterval(int milliseconds);
    void setActivityProbe(std::function<bool()> probe);

    Q_INVOKABLE void fetch();
    Q_INVOKABLE void pull();
    Q_INVOKABLE void pullWith(const QString &mode);
    Q_INVOKABLE void push();
    // The remote a branch with no upstream is published to, once the user
    // chose among several.
    Q_INVOKABLE void pushTo(const QString &remote);
    Q_INVOKABLE void forcePushWithLease(const QString &branch, const QString &remote);
    Q_INVOKABLE void pushTag(const QString &tag, const QString &remote = {});
    Q_INVOKABLE void setUpstream(const QString &branch, const QString &upstream);
    Q_INVOKABLE void unsetUpstream(const QString &branch);

signals:
    void settingsChanged();
    // The server refused the push because its branch moved; QML offers a
    // force with lease.
    void pushRejected(const QString &branch, const QString &remote, const QString &message);
    // Several remotes and none is the obvious one. `tag` is empty for a branch.
    void remoteChoiceNeeded(const QString &tag, const QStringList &remotes);

private:
    void fetchQuietly(bool quiet);
    void autoFetchTick();
    void pushBranch(const QString &branch, const QString &remote, bool forceWithLease);

    RepositoryController *m_repository;
    QTimer m_autoFetchTimer;
    QElapsedTimer m_sinceFetch;
    std::function<bool()> m_active;
    int m_interval = 10 * 60 * 1000;
    bool m_autoFetch = true;
    QString m_pullMode;
};

} // namespace GitNaga
