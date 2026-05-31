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
    items_          = listing.content;
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
    return static_cast<int>(items_.size());
}

QVariant PremiumizeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(items_.size())) {
        return {};
    }
    const auto& item = items_[static_cast<std::size_t>(index.row())];
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
    auto base = QAbstractListModel::flags(index);
    if (index.isValid()) {
        base |= Qt::ItemIsDragEnabled;
    }
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
        const auto& item = items_[static_cast<std::size_t>(idx.row())];
        QJsonObject obj;
        obj["id"]      = item.id;
        obj["name"]    = item.name;
        obj["isFolder"]= item.isFolder();
        if (item.link) {
            obj["link"] = *item.link;
        }
        if (item.size) {
            obj["size"] = *item.size;
        }
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
    return items_[static_cast<std::size_t>(row)];
}
