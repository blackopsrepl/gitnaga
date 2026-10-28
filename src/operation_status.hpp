#pragma once

#include "domain.hpp"

#include <QObject>
#include <QStringList>

namespace GitNaga {

// What git's state files say about a half-done operation, read off the GUI
// thread: which branch, onto what, which commit stopped it, how far along.
struct OperationFacts {
    QString kind;
    QString headName;
    QString onto;
    QString target;
    QString targetSubject;
    QString mergeMessage;
    QString bisectStart;
    int step = 0;
    int total = 0;
    bool mergeTool = false;
};

// The words the banner shows. Ours and theirs are named per operation
// because a rebase flips them: "ours" is the branch being rebased onto.
struct OperationText {
    QString title;
    QString detail;
    QString ours;
    QString theirs;
};

OperationFacts readOperationFacts(const RepositoryLocation &location, const QString &kind);
OperationText describeOperation(const OperationFacts &facts, const QString &currentBranch, const QVector<Ref> &refs);

// The in-progress operation as QML sees it (`repository.inProgress`).
class OperationStatus final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QString oursName READ oursName NOTIFY changed)
    Q_PROPERTY(QString theirsName READ theirsName NOTIFY changed)
    Q_PROPERTY(QStringList conflicts READ conflicts NOTIFY changed)
    Q_PROPERTY(int conflictCount READ conflictCount NOTIFY changed)
    Q_PROPERTY(bool canSkip READ canSkip NOTIFY changed)
    Q_PROPERTY(bool mergeToolConfigured READ mergeToolConfigured NOTIFY changed)

public:
    using QObject::QObject;

    QString kind() const { return m_facts.kind; }
    QString title() const { return m_text.title; }
    QString detail() const { return m_text.detail; }
    QString oursName() const { return m_text.ours; }
    QString theirsName() const { return m_text.theirs; }
    QStringList conflicts() const { return m_conflicts; }
    int conflictCount() const { return static_cast<int>(m_conflicts.size()); }
    bool canSkip() const;
    bool mergeToolConfigured() const { return m_facts.mergeTool; }

    // Re-read after every status change: a rebase step moves without the
    // operation kind changing.
    void update(const RepositorySnapshot &repository);

signals:
    void changed();

private:
    void apply(OperationFacts facts, QStringList conflicts, const RefSnapshot &refs);

    OperationFacts m_facts;
    OperationText m_text;
    QStringList m_conflicts;
    quint64 m_generation = 0;
};

} // namespace GitNaga
