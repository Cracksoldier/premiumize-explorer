#include "TransferProgressWindow.hpp"
#include "transfer/TransferManager.hpp"

#include <algorithm>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

TransferProgressWindow::TransferProgressWindow(TransferManager* manager, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::Tool)
    , manager_(manager)
{
    setWindowTitle("Transfers");
    setMinimumWidth(480);
    resize(520, 300);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::StyledPanel);
    auto* scrollContent = new QWidget;
    jobLayout_ = new QVBoxLayout(scrollContent);
    jobLayout_->setContentsMargins(4, 4, 4, 4);
    jobLayout_->setSpacing(0);
    jobLayout_->addStretch();
    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 0, 0, 0);

    clearBtn_ = new QToolButton(this);
    clearBtn_->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
    clearBtn_->setToolTip("Clear finished transfers");
    connect(clearBtn_, &QToolButton::clicked,
            this, &TransferProgressWindow::on_clearFinished_clicked);

    cancelAllBtn_ = new QPushButton("Cancel All", this);
    connect(cancelAllBtn_, &QPushButton::clicked,
            this, &TransferProgressWindow::on_cancelAll_clicked);

    btnRow->addWidget(clearBtn_);
    btnRow->addWidget(cancelAllBtn_, 1);
    outerLayout->addLayout(btnRow);

    updateButtonStates();

    connect(manager_, &TransferManager::jobStarted,  this, &TransferProgressWindow::on_jobStarted);
    connect(manager_, &TransferManager::jobProgress, this, &TransferProgressWindow::on_jobProgress);
    connect(manager_, &TransferManager::jobFinished, this, &TransferProgressWindow::on_jobFinished);
}

void TransferProgressWindow::on_jobStarted(int id, const QString& name, qint64 total)
{
    show();
    raise();

    if (!rows_.empty()) {
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        jobLayout_->insertWidget(jobLayout_->count() - 1, sep);
    }

    auto* container = new QWidget;
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(8, 6, 8, 6);

    auto* nameLabel    = new QLabel(name);
    nameLabel->setWordWrap(false);
    auto* progressBar  = new QProgressBar;
    progressBar->setRange(0, total > 0 ? static_cast<int>(total / 1024) : 0);
    auto* statsLabel   = new QLabel("Starting…");
    auto* elapsedLabel = new QLabel("Elapsed: 0s");

    vl->addWidget(nameLabel);
    vl->addWidget(progressBar);
    auto* hl = new QHBoxLayout;
    hl->addWidget(statsLabel);
    hl->addStretch();
    hl->addWidget(elapsedLabel);
    vl->addLayout(hl);

    jobLayout_->insertWidget(jobLayout_->count() - 1, container);

    rows_.push_back({id, nameLabel, progressBar, statsLabel, elapsedLabel, container, total, true});
    updateButtonStates();
}

void TransferProgressWindow::on_jobProgress(int id, qint64 bytes, qint64 total,
                                              qint64 elapsedMs, qint64 etaMs,
                                              double bytesPerSec)
{
    auto* row = findRow(id);
    if (!row) return;

    if (total > 0) {
        row->progressBar->setMaximum(static_cast<int>(total / 1024));
        row->progressBar->setValue(static_cast<int>(bytes / 1024));
    } else {
        row->progressBar->setMaximum(0);
    }

    row->statsLabel->setText(QStringLiteral("%1 / %2 — %3/s — ETA %4")
                                 .arg(formatBytes(bytes),
                                      formatBytes(total),
                                      formatBytes(static_cast<qint64>(bytesPerSec)),
                                      formatDuration(etaMs)));
    row->elapsedLabel->setText(QStringLiteral("Elapsed: %1").arg(formatDuration(elapsedMs)));
}

void TransferProgressWindow::on_jobFinished(int id, bool success, const QString& error)
{
    auto* row = findRow(id);
    if (!row) return;

    if (success) {
        row->progressBar->setValue(row->progressBar->maximum());
        const QString sizeStr = row->totalBytes > 0 ? formatBytes(row->totalBytes) : QString{};
        row->statsLabel->setText(sizeStr.isEmpty()
                                     ? QStringLiteral("Done")
                                     : QStringLiteral("Done · %1").arg(sizeStr));
    } else {
        row->statsLabel->setText(QStringLiteral("Failed: %1").arg(error));
    }
    row->elapsedLabel->clear();
    row->active = false;
    updateButtonStates();
}

void TransferProgressWindow::on_cancelAll_clicked()
{
    manager_->cancelAll();
    hide();
}

void TransferProgressWindow::on_clearFinished_clicked()
{
    std::vector<QWidget*> keep;
    for (const auto& r : rows_)
        if (r.active) keep.push_back(r.container);

    while (jobLayout_->count() > 1) {
        auto* item = jobLayout_->takeAt(0);
        if (auto* w = item->widget()) {
            const bool isKeep = std::find(keep.begin(), keep.end(), w) != keep.end();
            if (!isKeep) w->deleteLater();
        }
        delete item;
    }

    rows_.erase(std::remove_if(rows_.begin(), rows_.end(),
        [](const JobRow& r) { return !r.active; }), rows_.end());

    for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
        if (i > 0) {
            auto* sep = new QFrame;
            sep->setFrameShape(QFrame::HLine);
            sep->setFrameShadow(QFrame::Sunken);
            jobLayout_->insertWidget(jobLayout_->count() - 1, sep);
        }
        jobLayout_->insertWidget(jobLayout_->count() - 1, rows_[i].container);
    }

    updateButtonStates();
}

void TransferProgressWindow::updateButtonStates()
{
    const bool hasAny    = !rows_.empty();
    const bool hasActive = std::any_of(rows_.begin(), rows_.end(),
                               [](const JobRow& r) { return r.active; });
    cancelAllBtn_->setEnabled(hasActive);
    clearBtn_->setEnabled(hasAny);
}

TransferProgressWindow::JobRow* TransferProgressWindow::findRow(int jobId)
{
    for (auto& r : rows_) {
        if (r.jobId == jobId) return &r;
    }
    return nullptr;
}

QString TransferProgressWindow::formatBytes(qint64 bytes)
{
    if (bytes < 0)        return "?";
    if (bytes < 1024)     return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1 << 20)  return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1 << 30)  return QStringLiteral("%1 MB").arg(bytes / (1 << 20));
    return QStringLiteral("%.1f GB").arg(static_cast<double>(bytes) / (1 << 30));
}

QString TransferProgressWindow::formatDuration(qint64 ms)
{
    if (ms < 0) return "?";
    const auto s   = ms / 1000;
    const auto min = s / 60;
    const auto sec = s % 60;
    if (min > 0) return QStringLiteral("%1m %2s").arg(min).arg(sec);
    return QStringLiteral("%1s").arg(sec);
}
