#include "TransferManager.hpp"
#include "DownloadJob.hpp"
#include "UploadJob.hpp"
#include "api/PremiumizeApi.hpp"

#include <QFileInfo>

TransferManager::TransferManager(api::PremiumizeApi* api, QObject* parent)
    : QObject(parent)
    , api_(api)
{}

void TransferManager::enqueueUpload(const QStringList& localPaths,
                                     const QString& targetFolderId)
{
    for (const auto& path : localPaths) {
        const int id = nextJobId_++;
        const QFileInfo fi(path);
        const QString name = fi.fileName();
        const qint64  size = fi.size();

        retryActions_[id] = [this, path, targetFolderId]() {
            enqueueUpload({path}, targetFolderId);
        };

        queue_.push_back({id, name, size, [this, path, targetFolderId, id, name, size]() -> QObject* {
            auto* job = new UploadJob(path, targetFolderId, api_, this);
            emit jobStarted(id, name, size);
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

        emit jobQueued(id, name, size);
    }
    dispatchNext();
}

void TransferManager::enqueueDownload(const QString& remoteUrl,
                                       const QString& localDestPath,
                                       qint64         expectedSize,
                                       const QString& itemName)
{
    const int id = nextJobId_++;

    retryActions_[id] = [this, remoteUrl, localDestPath, expectedSize, itemName]() {
        enqueueDownload(remoteUrl, localDestPath, expectedSize, itemName);
    };

    queue_.push_back({id, itemName, expectedSize,
                      [this, remoteUrl, localDestPath, expectedSize, itemName, id]() -> QObject* {
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

    emit jobQueued(id, itemName, expectedSize);
    dispatchNext();
}

void TransferManager::cancelAll()
{
    for (const auto& pj : queue_)
        emit jobFinished(pj.id, false, "Cancelled");
    queue_.clear();

    const auto snapshot = active_;  // cancel() emits finished() synchronously, which modifies active_
    for (const auto& [id, obj] : snapshot) {
        if (auto* up = qobject_cast<UploadJob*>(obj)) up->cancel();
        else if (auto* dl = qobject_cast<DownloadJob*>(obj)) dl->cancel();
    }
}

void TransferManager::cancelJob(int jobId)
{
    // Check queue first
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        if (it->id == jobId) {
            queue_.erase(it);
            emit jobFinished(jobId, false, "Cancelled");
            return;
        }
    }

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

void TransferManager::retryJob(int jobId)
{
    if (!retryActions_.contains(jobId)) return;
    auto fn = retryActions_.take(jobId);
    fn();
}

void TransferManager::dispatchNext()
{
    while (!queue_.empty() &&
           static_cast<int>(active_.size()) < MaxConcurrent) {
        auto pj = queue_.front();
        queue_.pop_front();
        active_.push_back({pj.id, pj.factory()});
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
