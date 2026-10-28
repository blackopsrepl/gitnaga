#pragma once

#include "domain.hpp"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace GitNaga::detail {

// Lane assignment that can be fed one commit at a time, so a history read in
// chunks lays out exactly as it would have in one pass.
class GraphLayout
{
public:
    void place(Commit &commit);

private:
    QStringList m_lanes;
    QVector<int> m_laneColors;
    int m_nextColor = 0;
};

void assignGraphLayout(QVector<Commit> &commits);
QVector<DiffLine> parseDiff(const QByteArray &data);
QString decode(const QByteArray &value);

// `git status --porcelain=v2 -z --branch` into files and branch state. The
// operation in progress is read from the git directory, not from status.
WorktreeStatus parseStatus(const QByteArray &output);

} // namespace GitNaga::detail
