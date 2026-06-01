#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace api { class PremiumizeApi; }

class TransferManager : public QObject
{
    Q_OBJECT
public:
    static constexpr int MaxConcurrent = 2;

    explicit TransferManager(api::PremiumizeApi* api, QObject* parent = nullptr);

    void enqueueUpload(const QStringList& localPaths, const QString& targetFolderId);
    void enqueueDownload(const QString& remoteUrl,
                         const QString& localDestPath,
                         qint64         expectedSize,
                         const QString& itemName);
    void cancelAll();

signals:
    void jobStarted(int jobId, QString name, qint64 totalBytes);
    void jobProgress(int jobId, qint64 bytes, qint64 total,
                     qint64 elapsedMs, qint64 etaMs, double bytesPerSec);
    void jobFinished(int jobId, bool success, QString error);
    void uploadFinished(QString targetFolderId, bool success, QString error);
    void allFinished();

private:
    void dispatchNext();
    void onJobFinished(QObject* job, int jobId, bool success, const QString& error);

    api::PremiumizeApi*                              api_;
    std::deque<std::function<QObject*(int jobId)>>  queue_;
    std::vector<QObject*>                            active_;
    int                                              nextJobId_ = 0;
};
