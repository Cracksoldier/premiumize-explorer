#include "UploadJob.hpp"
#include "api/PremiumizeApi.hpp"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
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
        const auto doc = QJsonDocument::fromJson(infoReply->readAll());
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

    auto* multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart tokenPart;
    tokenPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant("form-data; name=\"token\""));
    tokenPart.setBody(info.token.toUtf8());
    multipart->append(tokenPart);

    QMimeDatabase mimeDb;
    const auto mime = mimeDb.mimeTypeForFile(localPath_).name();

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"%1\"")
                                    .arg(fileName())));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mime));
    filePart.setBodyDevice(file);
    file->setParent(multipart);
    multipart->append(filePart);

    QNetworkRequest req(QUrl(info.url));
    reply_ = uploadNam_.post(req, multipart);
    multipart->setParent(reply_);

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
    if (reply_->error() != QNetworkReply::NoError) {
        emit finished(false, reply_->errorString());
    } else {
        emit finished(true, {});
    }
    reply_ = nullptr;
}
