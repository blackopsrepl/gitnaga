#pragma once

#include "domain.hpp"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace GitNaga::detail {

void assignGraphLayout(QVector<Commit> &commits);
QVector<DiffLine> parseDiff(const QByteArray &data);
QString decode(const QByteArray &value);

} // namespace GitNaga::detail
