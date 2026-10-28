#pragma once

#include "domain.hpp"

#include <QByteArray>
#include <QSet>
#include <QString>
#include <QVector>

#include <expected>

namespace GitNaga::patch {

// Which way the patch is going to be applied. A forward patch adds the chosen
// lines to its base (staging from the unstaged diff); a reverse patch takes
// them back out of its base (unstaging from the staged diff, or discarding
// from the unstaged diff), so the lines left alone must already be there.
enum class Direction { Forward, Reverse };

// The change rows ('+' and '-') of the hunk whose header sits at `hunkRow`.
QSet<int> hunkChanges(const QVector<DiffLine> &lines, int hunkRow);

// Every change row of the file diff.
QSet<int> allChanges(const QVector<DiffLine> &lines);

// A minimal patch, for `git apply --recount`, that carries only the selected
// change rows of one file diff (the rows of `lines` as loadDiff returned
// them). Unselected changes become context where the base already holds the
// line and vanish where it does not; hunk ranges are recomputed. Fails, with
// a message for the user, when the diff cannot be split by line (binary,
// combined conflict diff, truncated) or nothing is selected.
std::expected<QByteArray, QString> build(const QVector<DiffLine> &lines, const QSet<int> &selected,
                                         Direction direction);

} // namespace GitNaga::patch
