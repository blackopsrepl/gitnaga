#include "split_diff_model.hpp"

#include "diff_line_model.hpp"

namespace GitNaga {

SplitDiffModel::SplitDiffModel(DiffLineModel *rows)
    : QAbstractListModel(rows)
    , m_rows(rows)
{
}

int SplitDiffModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows->document().split.size());
}

QVariant SplitDiffModel::data(const QModelIndex &index, int role) const
{
    const auto &document = m_rows->document();
    if (!index.isValid() || index.row() < 0 || index.row() >= document.split.size())
        return {};
    const auto &split = document.split.at(index.row());
    const auto *left = split.left >= 0 ? &document.rows.at(split.left) : nullptr;
    const auto *right = split.right >= 0 ? &document.rows.at(split.right) : nullptr;
    const bool spanning = left && left == right && left->kind == DiffLine::Kind::Hunk;
    // A blank side has kind -1, so the delegate can paint it as filler.
    const auto kind = [](const DiffRow *row) { return row ? static_cast<int>(row->kind) : -1; };
    const auto line = [](int number) { return number > 0 ? QVariant(number) : QVariant{}; };
    switch (role) {
    case HunkRole: return split.hunk;
    case SpanningRole: return spanning;
    case TextRole: return spanning ? left->content : QString();
    case LeftRowRole: return split.left;
    case LeftKindRole: return kind(left);
    case LeftLineRole: return left ? line(left->oldLine) : QVariant{};
    case LeftContentRole: return left ? left->content : QString();
    case LeftStyleRole: return left ? QVariant::fromValue(left->style) : QVariant{};
    case RightRowRole: return split.right;
    case RightKindRole: return kind(right);
    case RightLineRole: return right ? line(right->newLine) : QVariant{};
    case RightContentRole: return right ? right->content : QString();
    case RightStyleRole: return right ? QVariant::fromValue(right->style) : QVariant{};
    default: return {};
    }
}

QHash<int, QByteArray> SplitDiffModel::roleNames() const
{
    return { { HunkRole, "hunk" },
             { SpanningRole, "spanning" },
             { TextRole, "text" },
             { LeftRowRole, "leftRow" },
             { LeftKindRole, "leftKind" },
             { LeftLineRole, "leftLine" },
             { LeftContentRole, "leftContent" },
             { LeftStyleRole, "leftStyle" },
             { RightRowRole, "rightRow" },
             { RightKindRole, "rightKind" },
             { RightLineRole, "rightLine" },
             { RightContentRole, "rightContent" },
             { RightStyleRole, "rightStyle" } };
}

} // namespace GitNaga
