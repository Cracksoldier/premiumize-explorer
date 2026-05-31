#include "PremiumizeApi.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

namespace api {

static constexpr const char* kBaseUrl = "https://www.premiumize.me/api";

PremiumizeApi::PremiumizeApi(QObject* parent)
    : QObject(parent)
{}

void PremiumizeApi::setApiKey(const QString& key)
{
    apiKey_ = key;
}

bool PremiumizeApi::hasApiKey() const
{
    return !apiKey_.isEmpty();
}

QNetworkRequest PremiumizeApi::authorizedRequest(const QUrl& url) const
{
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(apiKey_).toUtf8());
    return req;
}

void PremiumizeApi::handleJsonReply(QNetworkReply* reply,
                                    std::function<void(const QJsonObject&)> handler)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, handler]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit networkError(QStringLiteral("Invalid JSON response"));
            return;
        }
        const auto obj = doc.object();
        const auto status = obj.value("status").toString();
        if (status != QStringLiteral("success")) {
            emit networkError(obj.value("message").toString("Unknown error"));
            return;
        }
        handler(obj);
    });
}

void PremiumizeApi::listFolder(const QString& folderId)
{
    QUrl url(QStringLiteral("%1/folder/list").arg(kBaseUrl));
    if (!folderId.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem("id", folderId);
        url.setQuery(query);
    }
    auto* reply = nam_.get(authorizedRequest(url));
    handleJsonReply(reply, [this, folderId](const QJsonObject& obj) {
        FolderListing listing;
        listing.folderId = folderId;
        listing.name     = obj.value("name").toString();
        listing.parentId = obj.value("parent_id").toString();

        const auto content = obj.value("content").toArray();
        for (const auto& val : content) {
            const auto item = val.toObject();
            FolderItem fi;
            fi.id   = item.value("id").toString();
            fi.name = item.value("name").toString();
            fi.type = (item.value("type").toString() == QStringLiteral("folder"))
                          ? ItemType::Folder
                          : ItemType::File;
            const auto createdRaw = item.value("created_at");
            if (!createdRaw.isNull()) {
                fi.createdAt = QDateTime::fromSecsSinceEpoch(
                    static_cast<qint64>(createdRaw.toDouble()));
            }
            const auto sizeVal = item.value("size");
            if (!sizeVal.isNull() && sizeVal.isDouble()) {
                fi.size = static_cast<qint64>(sizeVal.toDouble());
            }
            const auto mimeVal = item.value("mime_type");
            if (mimeVal.isString()) {
                fi.mimeType = mimeVal.toString();
            }
            const auto linkVal = item.value("link");
            if (linkVal.isString()) {
                fi.link = linkVal.toString();
            }
            listing.content.push_back(std::move(fi));
        }
        emit folderListingReady(std::move(listing));
    });
}

void PremiumizeApi::createFolder(const QString& name, const QString& parentId)
{
    QUrl url(QStringLiteral("%1/folder/create").arg(kBaseUrl));
    QUrlQuery query;
    query.addQueryItem("name", name);
    if (!parentId.isEmpty()) {
        query.addQueryItem("parent_id", parentId);
    }

    QNetworkRequest req = authorizedRequest(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    auto* reply = nam_.post(req, query.toString(QUrl::FullyEncoded).toUtf8());
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        emit folderCreated(obj.value("id").toString());
    });
}

void PremiumizeApi::deleteItem(const QString& id, bool isFolder)
{
    const QString endpoint = isFolder ? "folder/delete" : "item/delete";
    QUrl url(QStringLiteral("%1/%2").arg(kBaseUrl, endpoint));
    QUrlQuery query;
    query.addQueryItem("id", id);

    QNetworkRequest req = authorizedRequest(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    auto* reply = nam_.post(req, query.toString(QUrl::FullyEncoded).toUtf8());
    handleJsonReply(reply, [this](const QJsonObject&) {
        emit deleteFinished(true, {});
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit deleteFinished(false, reply->errorString());
        }
    });
}

void PremiumizeApi::pasteItems(const QString& targetFolderId,
                                const QStringList& fileIds,
                                const QStringList& folderIds)
{
    QUrl url(QStringLiteral("%1/folder/paste").arg(kBaseUrl));
    QUrlQuery query;
    query.addQueryItem("id", targetFolderId);
    for (const auto& id : fileIds) {
        query.addQueryItem("files[]", id);
    }
    for (const auto& id : folderIds) {
        query.addQueryItem("folders[]", id);
    }

    QNetworkRequest req = authorizedRequest(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    auto* reply = nam_.post(req, query.toString(QUrl::FullyEncoded).toUtf8());
    handleJsonReply(reply, [this](const QJsonObject&) {
        emit pasteFinished(true, {});
    });
}

void PremiumizeApi::requestUploadInfo(const QString& targetFolderId)
{
    QUrl url(QStringLiteral("%1/folder/uploadinfo").arg(kBaseUrl));
    if (!targetFolderId.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem("id", targetFolderId);
        url.setQuery(query);
    }
    auto* reply = nam_.get(authorizedRequest(url));
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        UploadInfo info;
        info.token = obj.value("token").toString();
        info.url   = obj.value("url").toString();
        emit uploadInfoReady(std::move(info));
    });
}

void PremiumizeApi::fetchAccountInfo()
{
    QUrl url(QStringLiteral("%1/account/info").arg(kBaseUrl));
    auto* reply = nam_.get(authorizedRequest(url));
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        AccountInfo info;
        info.spaceUsed  = static_cast<qint64>(obj.value("space_used").toDouble());
        info.spaceTotal = static_cast<qint64>(obj.value("limit_used").toDouble());
        emit accountInfoReady(info);
    });
}

QNetworkReply* PremiumizeApi::startDownload(const QString& url)
{
    return nam_.get(authorizedRequest(QUrl(url)));
}

} // namespace api
