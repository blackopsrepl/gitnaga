#pragma once

#include "commit_model.hpp"

#include <QPointer>
#include <QQuickItem>

namespace GitNaga {

class CommitGraphItem : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CommitGraph)
    Q_PROPERTY(CommitModel *model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged)
    Q_PROPERTY(qreal rowHeight READ rowHeight WRITE setRowHeight NOTIFY rowHeightChanged)
    Q_PROPERTY(int selectedRow READ selectedRow WRITE setSelectedRow NOTIFY selectedRowChanged)

public:
    explicit CommitGraphItem(QQuickItem *parent = nullptr);

    CommitModel *model() const;
    void setModel(CommitModel *model);
    qreal contentY() const;
    void setContentY(qreal contentY);
    qreal rowHeight() const;
    void setRowHeight(qreal rowHeight);
    int selectedRow() const;
    void setSelectedRow(int row);

signals:
    void modelChanged();
    void contentYChanged();
    void rowHeightChanged();
    void selectedRowChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    void synchronizeRows();

    QPointer<CommitModel> m_model;
    QVector<Commit> m_rows;
    qreal m_contentY = 0.0;
    qreal m_rowHeight = 58.0;
    int m_selectedRow = -1;
};

} // namespace GitNaga
