#pragma once
#include "ApiTypes.hpp"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <functional>

namespace api {

class PremiumizeApi : public QObject
{
    Q_OBJECT
public:
    explicit PremiumizeApi(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    bool hasApiKey() const;

    void listFolder(const QString& folderId = {});
    void createFolder(const QString& name, const QString& parentId = {});
    void deleteItem(const QString& id, bool isFolder);
    void pasteItems(const QString& targetFolderId,
                    const QStringList& fileIds,
                    const QStringList& folderIds);
    void requestUploadInfo(const QString& targetFolderId = {});
    void fetchAccountInfo();

    // Returns reply caller must connect to and delete when done
    QNetworkReply* startDownload(const QString& url);

signals:
    void folderListingReady(api::FolderListing listing);
    void folderCreated(QString newFolderId);
    void deleteFinished(bool success, QString errorMessage);
    void pasteFinished(bool success, QString errorMessage);
    void uploadInfoReady(api::UploadInfo info);
    void accountInfoReady(api::AccountInfo info);
    void networkError(QString message);

private:
    QNetworkRequest authorizedRequest(const QUrl& url) const;
    void handleJsonReply(QNetworkReply* reply,
                         std::function<void(const QJsonObject&)> handler);

    QNetworkAccessManager nam_;
    QString               apiKey_;
};

} // namespace api
