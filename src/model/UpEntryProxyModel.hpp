#pragma once
#include <QIdentityProxyModel>
#include <QModelIndex>

// QIdentityProxyModel wrapper that prepends a ".." row under the view's root
// index when not at the top level. Designed for use with a flat QListView +
// setRootIndex; not a general-purpose tree proxy.
class UpEntryProxyModel : public QIdentityProxyModel
{
    Q_OBJECT
public:
    explicit UpEntryProxyModel(QObject* parent = nullptr);

    // Set the source root and whether to show the up entry. Returns the proxy
    // root index to pass directly to view->setRootIndex().
    QModelIndex setSourceRoot(const QModelIndex& srcRoot, bool showUpEntry);

    bool isUpEntry(const QModelIndex& proxyIdx) const;

    int           rowCount(const QModelIndex& parent = {}) const override;
    QModelIndex   index(int row, int col, const QModelIndex& parent = {}) const override;
    QModelIndex   parent(const QModelIndex& child) const override;
    QVariant      data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& idx) const override;
    QMimeData*    mimeData(const QModelIndexList& indexes) const override;
    QModelIndex   mapToSource(const QModelIndex& proxyIdx) const override;
    QModelIndex   mapFromSource(const QModelIndex& srcIdx) const override;

private:
    QObject*              sentinel_;  // unique pointer used as internalPointer for the virtual row
    bool                  show_    = false;
    QModelIndex           viewRoot_;
};
