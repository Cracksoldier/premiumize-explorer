#include "TransferProgressWindow.hpp"
#include "transfer/TransferManager.hpp"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

TransferProgressWindow::TransferProgressWindow(TransferManager* manager, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::Tool)
    , manager_(manager)
{
    setWindowTitle("Transfers");
    setMinimumWidth(480);
    resize(520, 300);

    auto* outerLayout = new QVBoxLayout(this);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto* scrollContent = new QWidget;
    jobLayout_ = new QVBoxLayout(scrollContent);
    jobLayout_->addStretch();
    scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea);

    cancelAllBtn_ = new QPushButton("Cancel All", this);
    outerLayout->addWidget(cancelAllBtn_);

    connect(cancelAllBtn_, &QPushButton::clicked, this, &TransferProgressWindow::on_cancelAll_clicked);

    connect(manager_, &TransferManager::jobStarted,   this, &TransferProgressWindow::on_jobStarted);
    connect(manager_, &TransferManager::jobProgress,  this, &TransferProgressWindow::on_jobProgress);
    connect(manager_, &TransferManager::jobFinished,  this, &TransferProgressWindow::on_jobFinished);
}

void TransferProgressWindow::on_jobStarted(int id, const QString& name, qint64 total)
{
    show();
    raise();

    auto* container  = new QFrame;
    container->setFrameShape(QFrame::StyledPanel);
    auto* vl = new QVBoxLayout(container);

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

    // Insert before the stretch at the end
    jobLayout_->insertWidget(jobLayout_->count() - 1, container);

    rows_.push_back({id, nameLabel, progressBar, statsLabel, elapsedLabel, container});
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
        row->progressBar->setMaximum(0); // indeterminate
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
        row->statsLabel->setText("Done");
    } else {
        row->statsLabel->setText(QStringLiteral("Failed: %1").arg(error));
    }
    row->elapsedLabel->clear();
}

void TransferProgressWindow::on_cancelAll_clicked()
{
    manager_->cancelAll();
    hide();
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
