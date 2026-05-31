#pragma once
#include <QString>
#include <QDateTime>
#include <optional>
#include <vector>

namespace api {

enum class ItemType { File, Folder };

struct FolderItem {
    QString   id;
    QString   name;
    ItemType  type;
    QDateTime createdAt;
    std::optional<qint64>  size;
    std::optional<QString> mimeType;
    std::optional<QString> link;

    bool isFolder() const { return type == ItemType::Folder; }
};

struct FolderListing {
    QString folderId;
    QString name;
    QString parentId;
    std::vector<FolderItem> content;
};

struct UploadInfo {
    QString token;
    QString url;
};

struct AccountInfo {
    qint64 spaceUsed  = 0;
    qint64 spaceTotal = 0;
};

} // namespace api
