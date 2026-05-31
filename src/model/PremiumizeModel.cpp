#include "PremiumizeModel.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>

static constexpr const char* kCloudMime = "application/x-premiumize-items";

PremiumizeModel::PremiumizeModel(QObject* parent)
    : QAbstractListModel(parent)
{}

void PremiumizeModel::populate(const api::FolderListing& listing)
{
    beginResetModel();
    items_           = listing.content;
    currentFolderId_ = listing.folderId;
    parentFolderId_  = listing.parentId;
    endResetModel();
}

void PremiumizeModel::clear()
{
    beginResetModel();
    items_.clear();
    currentFolderId_.clear();
    parentFolderId_.clear();
    endResetModel();
}

int PremiumizeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(items_.size()) + (showUpEntry() ? 1 : 0);
}

QVariant PremiumizeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};
    if (showUpEntry() && index.row() == 0)
        return role == Qt::DisplayRole ? QStringLiteral("↑ Up") : QVariant{};
    const int src = index.row() - (showUpEntry() ? 1 : 0);
    const auto& item = items_[static_cast<std::size_t>(src)];
    switch (role) {
        case NameRole:     return item.name;
        case IdRole:       return item.id;
        case TypeRole:     return static_cast<int>(item.type);
        case SizeRole:     return item.size.value_or(-1);
        case CreatedAtRole:return item.createdAt;
        case LinkRole:     return item.link.value_or(QString{});
        case MimeTypeRole: return item.mimeType.value_or(QString{});
        default:           return {};
    }
}

Qt::ItemFlags PremiumizeModel::flags(const QModelIndex& index) const
{
    if (showUpEntry() && index.isValid() && index.row() == 0)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    auto base = QAbstractListModel::flags(index);
    if (index.isValid())
        base |= Qt::ItemIsDragEnabled;
    base |= Qt::ItemIsDropEnabled;
    return base;
}

Qt::DropActions PremiumizeModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList PremiumizeModel::mimeTypes() const
{
    return { kCloudMime, "text/uri-list" };
}

QMimeData* PremiumizeModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mime = new QMimeData;
    QJsonArray arr;
    for (const auto& idx : indexes) {
        if (!idx.isValid()) continue;
        if (showUpEntry() && idx.row() == 0) continue;
        const int src = idx.row() - (showUpEntry() ? 1 : 0);
        if (src < 0 || src >= static_cast<int>(items_.size())) continue;
        const auto& item = items_[static_cast<std::size_t>(src)];
        QJsonObject obj;
        obj["id"]       = item.id;
        obj["name"]     = item.name;
        obj["isFolder"] = item.isFolder();
        if (item.link) obj["link"] = *item.link;
        if (item.size) obj["size"] = *item.size;
        arr.append(obj);
    }
    mime->setData(kCloudMime, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return mime;
}

bool PremiumizeModel::canDropMimeData(const QMimeData* data, Qt::DropAction,
                                       int, int, const QModelIndex&) const
{
    return data->hasFormat("text/uri-list");
}

const api::FolderItem& PremiumizeModel::itemAt(int row) const
{
    Q_ASSERT(row >= 0 && row < static_cast<int>(items_.size()));
    return items_.at(static_cast<std::size_t>(row));
}

const api::FolderItem* PremiumizeModel::itemAtViewRow(int viewRow) const
{
    if (showUpEntry() && viewRow == 0) return nullptr;
    const int src = viewRow - (showUpEntry() ? 1 : 0);
    if (src < 0 || src >= static_cast<int>(items_.size())) return nullptr;
    return &items_.at(static_cast<std::size_t>(src));
}
