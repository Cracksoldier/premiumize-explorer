#pragma once
#include <QString>
#include <QDateTime>
#include <QList>
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

struct CloudTransferEntry {
    QString id;
    QString name;
    QString status;    // "waiting", "running", "finished", "seeding", "deleted", "error"
    float   progress = 0.f; // 0.0–1.0
    qint64  speedDown = 0;  // bytes/sec
    qint64  eta = -1;       // seconds remaining; -1 = unknown
    QString message;
    QString fileId;
};

} // namespace api
