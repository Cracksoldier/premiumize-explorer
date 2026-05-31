#include "TransferManager.hpp"
#include "DownloadJob.hpp"
#include "UploadJob.hpp"
#include "api/PremiumizeApi.hpp"

TransferManager::TransferManager(api::PremiumizeApi* api, QObject* parent)
    : QObject(parent)
    , api_(api)
{}

void TransferManager::enqueueUpload(const QStringList& localPaths,
                                     const QString& targetFolderId)
{
    for (const auto& path : localPaths) {
        const int id = nextJobId_++;
        queue_.push_back([this, path, targetFolderId, id](int /*jobId*/) -> QObject* {
            auto* job = new UploadJob(path, targetFolderId, api_, this);
            emit jobStarted(id, job->fileName(), job->totalBytes());
            connect(job, &UploadJob::progress, this,
                    [this, id](qint64 sent, qint64 total, qint64 elapsedMs,
                               qint64 etaMs, double bps) {
                        emit jobProgress(id, sent, total, elapsedMs, etaMs, bps);
                    });
            connect(job, &UploadJob::finished, this,
                    [this, id](bool ok, const QString& err) {
                        onJobFinished(id, ok, err);
                    });
            job->start();
            return job;
        });
    }
    dispatchNext();
}

void TransferManager::enqueueDownload(const QString& remoteUrl,
                                       const QString& localDestPath,
                                       qint64         expectedSize,
                                       const QString& itemName)
{
    const int id = nextJobId_++;
    queue_.push_back([this, remoteUrl, localDestPath, expectedSize, itemName, id](int) -> QObject* {
        auto* job = new DownloadJob(remoteUrl, localDestPath, expectedSize, api_, this);
        emit jobStarted(id, itemName, expectedSize);
        connect(job, &DownloadJob::progress, this,
                [this, id](qint64 recv, qint64 total, qint64 elapsedMs,
                           qint64 etaMs, double bps) {
                    emit jobProgress(id, recv, total, elapsedMs, etaMs, bps);
                });
        connect(job, &DownloadJob::finished, this,
                [this, id](bool ok, const QString& err) {
                    onJobFinished(id, ok, err);
                });
        job->start();
        return job;
    });
    dispatchNext();
}

void TransferManager::cancelAll()
{
    queue_.clear();
    for (auto* obj : active_) {
        if (auto* up = qobject_cast<UploadJob*>(obj)) {
            up->cancel();
        } else if (auto* dl = qobject_cast<DownloadJob*>(obj)) {
            dl->cancel();
        }
    }
}

void TransferManager::dispatchNext()
{
    while (!queue_.empty() &&
           static_cast<int>(active_.size()) < MaxConcurrent) {
        auto factory = queue_.front();
        queue_.pop_front();
        active_.push_back(factory(nextJobId_));
    }
}

void TransferManager::onJobFinished(int jobId, bool success, const QString& error)
{
    emit jobFinished(jobId, success, error);
    active_.erase(
        std::remove_if(active_.begin(), active_.end(),
                       [](QObject* o) { return o->property("_done").toBool(); }),
        active_.end());
    // Mark the sender as done
    if (auto* sender = qobject_cast<QObject*>(this->sender())) {
        sender->setProperty("_done", true);
        active_.erase(std::remove(active_.begin(), active_.end(), sender), active_.end());
        sender->deleteLater();
    }
    dispatchNext();
    if (active_.empty() && queue_.empty()) {
        emit allFinished();
    }
}
