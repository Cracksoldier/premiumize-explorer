#include "UploadJob.hpp"
#include "api/PremiumizeApi.hpp"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
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

    connect(api_, &api::PremiumizeApi::uploadInfoReady,
            this, &UploadJob::on_uploadInfoReady,
            Qt::SingleShotConnection);
    api_->requestUploadInfo(targetFolderId_);
    timer_.start();
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

void UploadJob::on_uploadInfoReady(api::UploadInfo info)
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
