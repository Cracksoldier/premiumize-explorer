#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QFile>
#include <QString>

class QNetworkReply;
namespace api { class PremiumizeApi; }

class DownloadJob : public QObject
{
    Q_OBJECT
public:
    explicit DownloadJob(QString remoteUrl,
                         QString localDestPath,
                         qint64  expectedSize,
                         api::PremiumizeApi* api,
                         QObject* parent = nullptr);

    void start();
    void cancel();

    QString fileName()           const;
    qint64  totalBytes()         const { return expectedSize_; }
    qint64  bytesTransferred()   const { return bytesReceived_; }

signals:
    void progress(qint64 received, qint64 total, qint64 elapsedMs, qint64 etaMs, double bytesPerSec);
    void finished(bool success, QString errorMessage);

private slots:
    void on_readyRead();
    void on_downloadProgress(qint64 received, qint64 total);
    void on_finished();

private:
    QString             remoteUrl_;
    QString             localDestPath_;
    qint64              expectedSize_;
    api::PremiumizeApi* api_;
    QNetworkReply*      reply_        = nullptr;
    QFile               outFile_;
    QElapsedTimer       timer_;
    qint64              bytesReceived_ = 0;
};
