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
        queue_.push_back({id, [this, path, targetFolderId, id]() -> QObject* {
            auto* job = new UploadJob(path, targetFolderId, api_, this);
            emit jobStarted(id, job->fileName(), job->totalBytes());
            connect(job, &UploadJob::progress, this,
                    [this, id](qint64 sent, qint64 total, qint64 elapsedMs,
                               qint64 etaMs, double bps) {
                        emit jobProgress(id, sent, total, elapsedMs, etaMs, bps);
                    });
            connect(job, &UploadJob::finished, this,
                    [this, job, id](bool ok, const QString& err) {
                        onJobFinished(job, id, ok, err);
                    });
            job->start();
            return job;
        }});
    }
    dispatchNext();
}

void TransferManager::enqueueDownload(const QString& remoteUrl,
                                       const QString& localDestPath,
                                       qint64         expectedSize,
                                       const QString& itemName)
{
    const int id = nextJobId_++;
    queue_.push_back({id, [this, remoteUrl, localDestPath, expectedSize, itemName, id]() -> QObject* {
        auto* job = new DownloadJob(remoteUrl, localDestPath, expectedSize, api_, this);
        emit jobStarted(id, itemName, expectedSize);
        connect(job, &DownloadJob::progress, this,
                [this, id](qint64 recv, qint64 total, qint64 elapsedMs,
                           qint64 etaMs, double bps) {
                    emit jobProgress(id, recv, total, elapsedMs, etaMs, bps);
                });
        connect(job, &DownloadJob::finished, this,
                [this, job, id](bool ok, const QString& err) {
                    onJobFinished(job, id, ok, err);
                });
        job->start();
        return job;
    }});
    dispatchNext();
}

void TransferManager::cancelAll()
{
    queue_.clear();
    const auto snapshot = active_;  // cancel() emits finished() synchronously, which modifies active_
    for (const auto& [id, obj] : snapshot) {
        if (auto* up = qobject_cast<UploadJob*>(obj)) up->cancel();
        else if (auto* dl = qobject_cast<DownloadJob*>(obj)) dl->cancel();
    }
}

void TransferManager::cancelJob(int jobId)
{
    QObject* target = nullptr;
    for (const auto& [id, obj] : active_) {
        if (id == jobId) { target = obj; break; }
    }
    if (!target) return;
    // Exit the loop before calling cancel(): cancel() emits finished() synchronously,
    // which calls onJobFinished() which erases from active_ — iterator must not be live.
    if (auto* dl = qobject_cast<DownloadJob*>(target)) dl->cancel();
    else if (auto* up = qobject_cast<UploadJob*>(target)) up->cancel();
}

void TransferManager::dispatchNext()
{
    while (!queue_.empty() &&
           static_cast<int>(active_.size()) < MaxConcurrent) {
        auto [id, factory] = queue_.front();
        queue_.pop_front();
        active_.push_back({id, factory()});
    }
}

void TransferManager::onJobFinished(QObject* job, int jobId, bool success, const QString& error)
{
    emit jobFinished(jobId, success, error);
    if (auto* up = qobject_cast<UploadJob*>(job)) {
        emit uploadFinished(up->targetFolderId(), success, error);
    }
    active_.erase(
        std::remove_if(active_.begin(), active_.end(),
                       [job](const auto& p) { return p.second == job; }),
        active_.end());
    job->deleteLater();
    dispatchNext();
    if (active_.empty() && queue_.empty()) {
        emit allFinished();
    }
}
