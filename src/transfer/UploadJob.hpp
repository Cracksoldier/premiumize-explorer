#pragma once
#include "api/ApiTypes.hpp"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QNetworkReply;
namespace api { class PremiumizeApi; }

class UploadJob : public QObject
{
    Q_OBJECT
public:
    explicit UploadJob(QString localPath,
                       QString targetFolderId,
                       api::PremiumizeApi* api,
                       QObject* parent = nullptr);

    void start();
    void cancel();

    QString fileName()           const;
    QString targetFolderId()     const { return targetFolderId_; }
    qint64  totalBytes()         const { return totalBytes_; }
    qint64  bytesTransferred()   const { return bytesTransferred_; }

signals:
    void progress(qint64 sent, qint64 total, qint64 elapsedMs, qint64 etaMs, double bytesPerSec);
    void finished(bool success, QString errorMessage);

private slots:
    void on_uploadProgress(qint64 sent, qint64 total);
    void on_uploadFinished();

private:
    void startUpload(api::UploadInfo info);
    QString              localPath_;
    QString              targetFolderId_;
    api::PremiumizeApi*  api_;
    QNetworkAccessManager uploadNam_;
    QNetworkReply*       reply_           = nullptr;
    QElapsedTimer        timer_;
    qint64               totalBytes_       = 0;
    qint64               bytesTransferred_ = 0;
    qint64               uploadStartMs_    = 0;
};
