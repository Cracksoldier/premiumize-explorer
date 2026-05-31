#include "UpEntryProxyModel.hpp"
#include <QMimeData>

UpEntryProxyModel::UpEntryProxyModel(QObject* parent)
    : QIdentityProxyModel(parent)
    , sentinel_(new QObject(this))
{}

QModelIndex UpEntryProxyModel::setSourceRoot(const QModelIndex& srcRoot, bool show)
{
    const QModelIndex proxyRoot = QIdentityProxyModel::mapFromSource(srcRoot);
    viewRoot_ = proxyRoot;
    show_     = show;
    return proxyRoot;
}

bool UpEntryProxyModel::isUpEntry(const QModelIndex& idx) const
{
    return idx.isValid() && idx.internalPointer() == sentinel_;
}

int UpEntryProxyModel::rowCount(const QModelIndex& parent) const
{
    return QIdentityProxyModel::rowCount(parent)
        + (show_ && parent == viewRoot_ ? 1 : 0);
}

QModelIndex UpEntryProxyModel::index(int row, int col, const QModelIndex& parent) const
{
    if (show_ && parent == viewRoot_) {
        if (row == 0) return createIndex(0, col, sentinel_);
        const QModelIndex srcParent = QIdentityProxyModel::mapToSource(viewRoot_);
        return mapFromSource(sourceModel()->index(row - 1, col, srcParent));
    }
    return QIdentityProxyModel::index(row, col, parent);
}

QModelIndex UpEntryProxyModel::parent(const QModelIndex& child) const
{
    if (isUpEntry(child)) return viewRoot_;
    return QIdentityProxyModel::parent(child);
}

QVariant UpEntryProxyModel::data(const QModelIndex& idx, int role) const
{
    if (isUpEntry(idx))
        return role == Qt::DisplayRole ? QStringLiteral("↑ Up") : QVariant{};
    return QIdentityProxyModel::data(idx, role);
}

Qt::ItemFlags UpEntryProxyModel::flags(const QModelIndex& idx) const
{
    if (isUpEntry(idx))
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return QIdentityProxyModel::flags(idx);
}

QMimeData* UpEntryProxyModel::mimeData(const QModelIndexList& indexes) const
{
    QModelIndexList srcIndexes;
    for (const auto& idx : indexes) {
        if (!isUpEntry(idx))
            srcIndexes << mapToSource(idx);
    }
    return sourceModel()->mimeData(srcIndexes);
}

QModelIndex UpEntryProxyModel::mapToSource(const QModelIndex& proxyIdx) const
{
    if (!proxyIdx.isValid()) return {};
    if (isUpEntry(proxyIdx)) return {};
    if (show_ && viewRoot_.isValid()) {
        if (proxyIdx == viewRoot_)
            return QIdentityProxyModel::mapToSource(proxyIdx);
        const QModelIndex srcRoot = QIdentityProxyModel::mapToSource(viewRoot_);
        return sourceModel()->index(proxyIdx.row() - 1, proxyIdx.column(), srcRoot);
    }
    return QIdentityProxyModel::mapToSource(proxyIdx);
}

QModelIndex UpEntryProxyModel::mapFromSource(const QModelIndex& srcIdx) const
{
    if (!srcIdx.isValid()) return {};
    if (show_ && viewRoot_.isValid()) {
        const QModelIndex srcRoot = QIdentityProxyModel::mapToSource(viewRoot_);
        if (srcIdx.parent() == srcRoot)
            return createIndex(srcIdx.row() + 1, srcIdx.column(), srcIdx.internalPointer());
    }
    return QIdentityProxyModel::mapFromSource(srcIdx);
}
