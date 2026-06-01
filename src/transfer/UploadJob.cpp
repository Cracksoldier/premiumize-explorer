#include "UploadJob.hpp"
#include "api/PremiumizeApi.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>

UploadJob::UploadJob(QString localPath,
                     QString targetFolderId,
                     api::PremiumizeApi* api,
                     QObject* parent)
    : QObject(parent)
    , localPath_(std::move(localPath))
    , targetFolderId_(std::move(targetFolderId))
    , api_(api)
{}

void UploadJob::start()
{
    QFileInfo fi(localPath_);
    totalBytes_ = fi.size();
    timer_.start();

    auto* infoReply = api_->fetchUploadInfo(targetFolderId_);
    connect(infoReply, &QNetworkReply::finished, this, [this, infoReply]() {
        infoReply->deleteLater();
        if (infoReply->error() != QNetworkReply::NoError) {
            emit finished(false, infoReply->errorString());
            return;
        }
        const auto raw = infoReply->readAll();
        const auto doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) {
            emit finished(false, QStringLiteral("Invalid upload-info response"));
            return;
        }
        const auto obj = doc.object();
        if (obj.value("status").toString() != QStringLiteral("success")) {
            emit finished(false, obj.value("message").toString("Upload info request failed"));
            return;
        }
        api::UploadInfo info;
        info.token = obj.value("token").toString();
        info.url   = obj.value("url").toString();
        if (info.token.isEmpty() || info.url.isEmpty()) {
            emit finished(false, QStringLiteral("Empty token or URL in upload-info response"));
            return;
        }
        api_->log(QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ")
                  + QStringLiteral("  uploadinfo: %1").arg(QString::fromUtf8(raw)));
        startUpload(std::move(info));
    });
}

void UploadJob::cancel()
{
    if (reply_) {
        reply_->abort();
    }
}

QString UploadJob::fileName() const
{
    return QFileInfo(localPath_).fileName();
}

void UploadJob::startUpload(api::UploadInfo info)
{
    auto* file = new QFile(localPath_);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        emit finished(false, QStringLiteral("Cannot open file: %1").arg(localPath_));
        return;
    }
    const auto fileData = file->readAll();
    delete file;

    QMimeDatabase mimeDb;
    const auto mime = mimeDb.mimeTypeForFile(localPath_).name().toUtf8();

    const QByteArray boundary = "----PremiumizeFormBoundary7MA4YWxkTrZu0gW";
    const QByteArray crlf     = "\r\n";
    const QByteArray dash     = "--";

    QByteArray body;
    body += dash + boundary + crlf;
    body += "Content-Disposition: form-data; name=\"token\"" + crlf + crlf;
    body += info.token.toUtf8() + crlf;
    body += dash + boundary + crlf;
    body += "Content-Disposition: form-data; name=\"file\"; filename=\""
            + fileName().toUtf8() + "\"" + crlf;
    body += "Content-Type: " + mime + crlf + crlf;
    body += fileData + crlf;
    body += dash + boundary + dash + crlf;

    QNetworkRequest req(QUrl(info.url));
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("multipart/form-data; boundary=") + boundary);
    req.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

    uploadStartMs_ = QDateTime::currentMSecsSinceEpoch();
    api_->log(QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ")
              + QStringLiteral("→ POST %1 (upload: %2)").arg(info.url, fileName()));
    reply_ = uploadNam_.post(req, body);

    connect(reply_, &QNetworkReply::uploadProgress, this, &UploadJob::on_uploadProgress);
    connect(reply_, &QNetworkReply::finished,        this, &UploadJob::on_uploadFinished);
}

void UploadJob::on_uploadProgress(qint64 sent, qint64 total)
{
    bytesTransferred_ = sent;
    if (total > 0) totalBytes_ = total;
    const qint64 elapsedMs = timer_.elapsed();
    const double bytesPerSec = elapsedMs > 0
        ? (static_cast<double>(sent) / elapsedMs * 1000.0)
        : 0.0;
    const qint64 remaining = (total > 0 && bytesPerSec > 0)
        ? static_cast<qint64>((total - sent) / bytesPerSec * 1000.0)
        : -1;
    emit progress(sent, total, elapsedMs, remaining, bytesPerSec);
}

void UploadJob::on_uploadFinished()
{
    reply_->deleteLater();
    const qint64 ms = QDateTime::currentMSecsSinceEpoch() - uploadStartMs_;
    const auto responseBody = reply_->readAll();
    if (reply_->error() != QNetworkReply::NoError) {
        api_->log(QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ")
                  + QStringLiteral("← ERR (%1ms) %2 | %3")
                    .arg(ms)
                    .arg(reply_->errorString())
                    .arg(QString::fromUtf8(responseBody.left(300))));
        emit finished(false, reply_->errorString());
    } else {
        api_->log(QDateTime::currentDateTime().toString("[HH:mm:ss.zzz] ")
                  + QStringLiteral("← %1 (%2ms) upload complete")
                    .arg(reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                    .arg(ms));
        emit finished(true, {});
    }
    reply_ = nullptr;
}
