#include "commit_graph_item.hpp"

#include <QColor>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>

#include <array>
#include <cmath>

namespace GitNaga {
namespace {

constexpr qreal laneSpacing = 18.0;
constexpr qreal leftPadding = 18.0;
constexpr int circleSegments = 12;

const std::array<QColor, 8> colors = {
    QColor(QStringLiteral("#78a9ff")), QColor(QStringLiteral("#55d6be")),
    QColor(QStringLiteral("#f4c95d")), QColor(QStringLiteral("#ef8354")),
    QColor(QStringLiteral("#b892ff")), QColor(QStringLiteral("#f284b6")),
    QColor(QStringLiteral("#72ddf7")), QColor(QStringLiteral("#a8c686")),
};

qreal laneX(int lane)
{
    return leftPadding + static_cast<qreal>(lane) * laneSpacing;
}

QSGGeometryNode *lineNode(const QVector<QPointF> &points, const QColor &color)
{
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), static_cast<int>(points.size()));
    geometry->setDrawingMode(QSGGeometry::DrawLines);
    geometry->setLineWidth(2.0F);
    auto *vertices = geometry->vertexDataAsPoint2D();
    for (qsizetype index = 0; index < points.size(); ++index)
        vertices[index].set(static_cast<float>(points.at(index).x()), static_cast<float>(points.at(index).y()));

    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);
    auto *node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QSGGeometryNode *circleNode(const QPointF &center, qreal radius, const QColor &color)
{
    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), circleSegments + 2);
    geometry->setDrawingMode(QSGGeometry::DrawTriangleFan);
    auto *vertices = geometry->vertexDataAsPoint2D();
    vertices[0].set(static_cast<float>(center.x()), static_cast<float>(center.y()));
    for (int index = 0; index <= circleSegments; ++index) {
        const qreal angle = (static_cast<qreal>(index) / circleSegments) * 2.0 * M_PI;
        vertices[index + 1].set(static_cast<float>(center.x() + std::cos(angle) * radius),
                                static_cast<float>(center.y() + std::sin(angle) * radius));
    }
    auto *material = new QSGFlatColorMaterial;
    material->setColor(color);
    auto *node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

} // namespace

CommitGraphItem::CommitGraphItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

CommitModel *CommitGraphItem::model() const { return m_model; }

void CommitGraphItem::setModel(CommitModel *model)
{
    if (m_model == model)
        return;
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    m_model = model;
    if (m_model) {
        connect(m_model, &QAbstractItemModel::modelReset, this, &CommitGraphItem::synchronizeRows);
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &CommitGraphItem::synchronizeRows);
    }
    synchronizeRows();
    emit modelChanged();
}

qreal CommitGraphItem::contentY() const { return m_contentY; }
void CommitGraphItem::setContentY(qreal value)
{
    if (qFuzzyCompare(m_contentY, value)) return;
    m_contentY = value;
    update();
    emit contentYChanged();
}
qreal CommitGraphItem::rowHeight() const { return m_rowHeight; }
void CommitGraphItem::setRowHeight(qreal value)
{
    if (qFuzzyCompare(m_rowHeight, value)) return;
    m_rowHeight = value;
    update();
    emit rowHeightChanged();
}
int CommitGraphItem::selectedRow() const { return m_selectedRow; }
void CommitGraphItem::setSelectedRow(int row)
{
    if (m_selectedRow == row) return;
    m_selectedRow = row;
    update();
    emit selectedRowChanged();
}

void CommitGraphItem::synchronizeRows()
{
    m_rows = m_model ? m_model->commits() : QVector<Commit>{};
    update();
}

QSGNode *CommitGraphItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;
    auto *root = new QSGNode;
    if (m_rows.isEmpty() || m_rowHeight <= 0.0)
        return root;

    const int first = std::max(0, static_cast<int>(std::floor(m_contentY / m_rowHeight)) - 1);
    const int last = std::min(static_cast<int>(m_rows.size()) - 1,
                              static_cast<int>(std::ceil((m_contentY + height()) / m_rowHeight)) + 1);
    std::array<QVector<QPointF>, colors.size()> lines;

    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        const qreal y = static_cast<qreal>(row) * m_rowHeight - m_contentY + m_rowHeight / 2.0;
        const qreal nextY = y + m_rowHeight;
        for (const auto &segment : commit.segments) {
            auto &vertices = lines.at(static_cast<size_t>(segment.fromLane) % colors.size());
            const QPointF from(laneX(segment.fromLane), y);
            const QPointF to(laneX(segment.toLane), nextY);
            if (segment.fromLane == segment.toLane) {
                vertices << from << to;
            } else {
                const qreal middle = y + m_rowHeight * 0.55;
                vertices << from << QPointF(from.x(), middle)
                         << QPointF(from.x(), middle) << QPointF(to.x(), nextY);
            }
        }
    }

    for (size_t index = 0; index < colors.size(); ++index) {
        if (!lines[index].isEmpty())
            root->appendChildNode(lineNode(lines[index], colors[index]));
    }
    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        const qreal y = static_cast<qreal>(row) * m_rowHeight - m_contentY + m_rowHeight / 2.0;
        const auto color = colors.at(static_cast<size_t>(commit.lane) % colors.size());
        if (row == m_selectedRow)
            root->appendChildNode(circleNode({ laneX(commit.lane), y }, 8.0, QColor(QStringLiteral("#ffffff"))));
        root->appendChildNode(circleNode({ laneX(commit.lane), y }, 5.0, color));
    }
    return root;
}

} // namespace GitNaga
