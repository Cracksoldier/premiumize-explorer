#include "PremiumizeApi.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

namespace api {

static constexpr const char* kBaseUrl = "https://www.premiumize.me/api";

static QString ts()
{
    return QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ");
}

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
                                    std::function<void(const QJsonObject&)> handler,
                                    qint64 startMs)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, handler, startMs]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (startMs >= 0)
                emit requestLogged(ts() + QStringLiteral("← ERR (%1ms) %2")
                    .arg(QDateTime::currentMSecsSinceEpoch() - startMs)
                    .arg(reply->errorString()));
            emit networkError(reply->errorString());
            return;
        }
        const auto raw = reply->readAll();
        const auto doc = QJsonDocument::fromJson(raw);
        if (startMs >= 0) {
            emit requestLogged(ts() + QStringLiteral("← %1 (%2ms) %3")
                .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                .arg(QDateTime::currentMSecsSinceEpoch() - startMs)
                .arg(QString::fromUtf8(raw.left(200))));
        }
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
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ GET %1").arg(url.toString()));
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
    }, startMs);
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
    const QString body = query.toString(QUrl::FullyEncoded);
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ POST %1\n  Body: %2").arg(url.toString(), body));
    auto* reply = nam_.post(req, body.toUtf8());
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        emit folderCreated(obj.value("id").toString());
    }, startMs);
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
    const QString body = query.toString(QUrl::FullyEncoded);
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ POST %1\n  Body: %2").arg(url.toString(), body));
    auto* reply = nam_.post(req, body.toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, startMs]() {
        reply->deleteLater();
        const qint64 ms = QDateTime::currentMSecsSinceEpoch() - startMs;
        if (reply->error() != QNetworkReply::NoError) {
            emit requestLogged(ts() + QStringLiteral("← ERR (%1ms) %2").arg(ms).arg(reply->errorString()));
            emit deleteFinished(false, reply->errorString());
            return;
        }
        const auto raw = reply->readAll();
        const auto doc = QJsonDocument::fromJson(raw);
        const auto obj = doc.isObject() ? doc.object() : QJsonObject{};
        emit requestLogged(ts() + QStringLiteral("← %1 (%2ms) %3")
            .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
            .arg(ms)
            .arg(QString::fromUtf8(raw.left(200))));
        if (obj.value("status").toString() != QStringLiteral("success")) {
            emit deleteFinished(false, obj.value("message").toString("Delete failed"));
            return;
        }
        emit deleteFinished(true, {});
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
    const QString body = query.toString(QUrl::FullyEncoded);
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ POST %1\n  Body: %2").arg(url.toString(), body));
    auto* reply = nam_.post(req, body.toUtf8());
    handleJsonReply(reply, [this](const QJsonObject&) {
        emit pasteFinished(true, {});
    }, startMs);
}

void PremiumizeApi::requestUploadInfo(const QString& targetFolderId)
{
    QUrl url(QStringLiteral("%1/folder/uploadinfo").arg(kBaseUrl));
    if (!targetFolderId.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem("id", targetFolderId);
        url.setQuery(query);
    }
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ GET %1").arg(url.toString()));
    auto* reply = nam_.get(authorizedRequest(url));
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        UploadInfo info;
        info.token = obj.value("token").toString();
        info.url   = obj.value("url").toString();
        emit uploadInfoReady(std::move(info));
    }, startMs);
}

void PremiumizeApi::fetchAccountInfo()
{
    QUrl url(QStringLiteral("%1/account/info").arg(kBaseUrl));
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ GET %1").arg(url.toString()));
    auto* reply = nam_.get(authorizedRequest(url));
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        AccountInfo info;
        info.spaceUsed  = static_cast<qint64>(obj.value("space_used").toDouble());
        info.spaceTotal = static_cast<qint64>(obj.value("limit_used").toDouble());
        emit accountInfoReady(info);
    }, startMs);
}

void PremiumizeApi::fetchTransferList()
{
    QUrl url(QStringLiteral("%1/transfer/list").arg(kBaseUrl));
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ GET %1").arg(url.toString()));
    auto* reply = nam_.get(authorizedRequest(url));
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        QList<CloudTransferEntry> entries;
        const auto arr = obj.value("transfers").toArray();
        for (const auto& val : arr) {
            const auto item = val.toObject();
            CloudTransferEntry e;
            e.id        = item.value("id").toString();
            e.name      = item.value("name").toString();
            e.status    = item.value("status").toString();
            e.message   = item.value("message").toString();
            e.fileId    = item.value("file_id").toString();
            e.speedDown = static_cast<qint64>(item.value("speed_down").toDouble());
            e.eta       = static_cast<qint64>(item.value("eta").toDouble(-1));
            float p = static_cast<float>(item.value("progress").toDouble());
            if (p > 1.0f) p /= 100.0f;
            e.progress = p;
            entries.append(std::move(e));
        }
        emit transferListReady(std::move(entries));
    }, startMs);
}

void PremiumizeApi::searchItems(const QString& query)
{
    QUrl url(QStringLiteral("%1/folder/search").arg(kBaseUrl));
    QUrlQuery q;
    q.addQueryItem("q", query);
    url.setQuery(q);
    const qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    emit requestLogged(ts() + QStringLiteral("→ GET %1").arg(url.toString()));
    auto* reply = nam_.get(authorizedRequest(url));
    handleJsonReply(reply, [this](const QJsonObject& obj) {
        QList<FolderItem> results;
        const auto content = obj.value("content").toArray();
        for (const auto& val : content) {
            const auto item = val.toObject();
            if (item.value("type").toString() != QStringLiteral("file")) continue;
            FolderItem fi;
            fi.id   = item.value("id").toString();
            fi.name = item.value("name").toString();
            fi.type = ItemType::File;
            const auto createdRaw = item.value("created_at");
            if (!createdRaw.isNull())
                fi.createdAt = QDateTime::fromSecsSinceEpoch(
                    static_cast<qint64>(createdRaw.toDouble()));
            const auto sizeVal = item.value("size");
            if (!sizeVal.isNull() && sizeVal.isDouble())
                fi.size = static_cast<qint64>(sizeVal.toDouble());
            const auto mimeVal = item.value("mime_type");
            if (mimeVal.isString()) fi.mimeType = mimeVal.toString();
            const auto linkVal = item.value("link");
            if (linkVal.isString()) fi.link = linkVal.toString();
            results.append(std::move(fi));
        }
        emit searchResultsReady(std::move(results));
    }, startMs);
}

QNetworkReply* PremiumizeApi::startDownload(const QString& url)
{
    emit requestLogged(ts() + QStringLiteral("→ GET %1").arg(url));
    return nam_.get(authorizedRequest(QUrl(url)));
}

QNetworkReply* PremiumizeApi::fetchUploadInfo(const QString& targetFolderId)
{
    QUrl url(QStringLiteral("%1/folder/uploadinfo").arg(kBaseUrl));
    if (!targetFolderId.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem("id", targetFolderId);
        url.setQuery(query);
    }
    emit requestLogged(ts() + QStringLiteral("→ GET %1").arg(url.toString()));
    return nam_.get(authorizedRequest(url));
}

} // namespace api
