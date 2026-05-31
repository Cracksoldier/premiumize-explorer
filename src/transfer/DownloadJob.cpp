#include "DownloadJob.hpp"
#include "api/PremiumizeApi.hpp"

#include <QFileInfo>
#include <QNetworkReply>

DownloadJob::DownloadJob(QString remoteUrl,
                         QString localDestPath,
                         qint64  expectedSize,
                         api::PremiumizeApi* api,
                         QObject* parent)
    : QObject(parent)
    , remoteUrl_(std::move(remoteUrl))
    , localDestPath_(std::move(localDestPath))
    , expectedSize_(expectedSize)
    , api_(api)
    , outFile_(localDestPath_)
{}

void DownloadJob::start()
{
    if (!outFile_.open(QIODevice::WriteOnly)) {
        emit finished(false, QStringLiteral("Cannot open file for writing: %1")
                                 .arg(outFile_.fileName()));
        return;
    }
    timer_.start();
    reply_ = api_->startDownload(remoteUrl_);
    connect(reply_, &QNetworkReply::readyRead,        this, &DownloadJob::on_readyRead);
    connect(reply_, &QNetworkReply::downloadProgress, this, &DownloadJob::on_downloadProgress);
    connect(reply_, &QNetworkReply::finished,         this, &DownloadJob::on_finished);
}

void DownloadJob::cancel()
{
    if (reply_) {
        reply_->abort();
    }
}

QString DownloadJob::fileName() const
{
    return QFileInfo(localDestPath_).fileName();
}

void DownloadJob::on_readyRead()
{
    outFile_.write(reply_->readAll());
}

void DownloadJob::on_downloadProgress(qint64 received, qint64 total)
{
    bytesReceived_ = received;
    const qint64 elapsedMs = timer_.elapsed();
    const double bytesPerSec = elapsedMs > 0
        ? (static_cast<double>(received) / elapsedMs * 1000.0)
        : 0.0;
    const qint64 remaining = (total > 0 && bytesPerSec > 0)
        ? static_cast<qint64>((total - received) / bytesPerSec * 1000.0)
        : -1;
    emit progress(received, total, elapsedMs, remaining, bytesPerSec);
}

void DownloadJob::on_finished()
{
    outFile_.close();
    reply_->deleteLater();
    if (reply_->error() != QNetworkReply::NoError) {
        outFile_.remove();
        emit finished(false, reply_->errorString());
    } else {
        emit finished(true, {});
    }
    reply_ = nullptr;
}
