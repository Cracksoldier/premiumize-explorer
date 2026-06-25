#include "TransferProgressWindow.hpp"
#include "FormatHelpers.hpp"
#include "config/AppConfig.hpp"
#include "transfer/TransferManager.hpp"

#include <algorithm>
#include <QCheckBox>
#include <QGuiApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

TransferProgressWindow::TransferProgressWindow(TransferManager* manager, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::Tool)
    , manager_(manager)
{
    setWindowTitle("Transfers");
    setMinimumWidth(520);
    resize(560, 360);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::StyledPanel);
    auto* scrollContent = new QWidget;
    jobLayout_ = new QVBoxLayout(scrollContent);
    jobLayout_->setContentsMargins(4, 4, 4, 4);
    jobLayout_->setSpacing(0);
    jobLayout_->addStretch();
    scrollArea_->setWidget(scrollContent);
    outerLayout->addWidget(scrollArea_);

    // Status bar
    statusBar_ = new QLabel("Idle", this);
    statusBar_->setContentsMargins(2, 0, 2, 0);
    outerLayout->addWidget(statusBar_);

    // Bottom controls
    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(4);

    autoScrollCheck_ = new QCheckBox("Auto-scroll", this);
    autoScrollCheck_->setChecked(true);

    stayOnTopCheck_ = new QCheckBox("Stay on top", this);
    if (QGuiApplication::platformName() == "wayland") {
        stayOnTopCheck_->setEnabled(false);
        stayOnTopCheck_->setToolTip("Not supported under Wayland");
    } else {
        const bool onTop = AppConfig::instance().transfersStayOnTop();
        stayOnTopCheck_->setChecked(onTop);
        if (onTop)
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    }

    clearBtn_ = new QToolButton(this);
    clearBtn_->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
    clearBtn_->setToolTip("Clear finished transfers");
    connect(clearBtn_, &QToolButton::clicked,
            this, &TransferProgressWindow::on_clearFinished_clicked);

    cancelAllBtn_ = new QPushButton("Cancel All", this);
    connect(cancelAllBtn_, &QPushButton::clicked,
            this, &TransferProgressWindow::on_cancelAll_clicked);

    btnRow->addWidget(autoScrollCheck_);
    btnRow->addWidget(stayOnTopCheck_);
    btnRow->addStretch();
    btnRow->addWidget(clearBtn_);
    btnRow->addWidget(cancelAllBtn_);
    outerLayout->addLayout(btnRow);

    updateButtonStates();
    updateStatusBar();

    // Detect manual scroll — uncheck auto-scroll
    connect(scrollArea_->verticalScrollBar(), &QScrollBar::sliderPressed,
            this, [this]() { autoScrollCheck_->setChecked(false); });
    scrollArea_->viewport()->installEventFilter(this);

    connect(autoScrollCheck_, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        if (state == Qt::Checked)
            scrollToActive();
    });

    connect(stayOnTopCheck_, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        const bool onTop = (state == Qt::Checked);
        AppConfig::instance().setTransfersStayOnTop(onTop);
        setWindowFlags(onTop ? windowFlags() | Qt::WindowStaysOnTopHint
                             : windowFlags() & ~Qt::WindowStaysOnTopHint);
        show(); // required after setWindowFlags()
    });

    // Permanent handler: fires whenever content grows or shrinks (new job added,
    // rows cleared, window resized). A single connection avoids the per-enqueue
    // SingleShotConnection accumulation problem.
    connect(scrollArea_->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, [this](int, int) {
                if (autoScrollCheck_->isChecked())
                    scrollToActive();
            });

    connect(manager_, &TransferManager::jobQueued,   this, &TransferProgressWindow::on_jobQueued);
    connect(manager_, &TransferManager::jobStarted,  this, &TransferProgressWindow::on_jobStarted);
    connect(manager_, &TransferManager::jobProgress, this, &TransferProgressWindow::on_jobProgress);
    connect(manager_, &TransferManager::jobFinished, this, &TransferProgressWindow::on_jobFinished);
    connect(manager_, &TransferManager::allFinished, this, &TransferProgressWindow::on_allFinished);
}

bool TransferProgressWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == scrollArea_->viewport() && event->type() == QEvent::Wheel)
        autoScrollCheck_->setChecked(false);
    return QWidget::eventFilter(watched, event);
}

void TransferProgressWindow::on_jobQueued(int id, const QString& name, qint64 total)
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
    vl->setSpacing(2);

    // Top row: icon + name + cancel/retry buttons
    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(4);

    auto* statusIcon = new QLabel;
    statusIcon->setFixedSize(16, 16);
    statusIcon->setAlignment(Qt::AlignCenter);

    auto* nameLabel = new QLabel(name);
    nameLabel->setWordWrap(false);

    auto* cancelBtn = new QToolButton;
    cancelBtn->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    cancelBtn->setToolTip("Cancel");
    cancelBtn->setAutoRaise(true);

    auto* retryBtn = new QToolButton;
    retryBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    retryBtn->setToolTip("Retry");
    retryBtn->setAutoRaise(true);
    retryBtn->hide();

    topRow->addWidget(statusIcon);
    topRow->addWidget(nameLabel, 1);
    topRow->addWidget(cancelBtn);
    topRow->addWidget(retryBtn);
    vl->addLayout(topRow);

    auto* progressBar = new QProgressBar;
    progressBar->setRange(0, total > 0 ? static_cast<int>(total / 1024) : 0);
    vl->addWidget(progressBar);

    auto* statsRow = new QHBoxLayout;
    auto* statsLabel = new QLabel("Queued");
    auto* elapsedLabel = new QLabel;
    statsRow->addWidget(statsLabel);
    statsRow->addStretch();
    statsRow->addWidget(elapsedLabel);
    vl->addLayout(statsRow);

    jobLayout_->insertWidget(jobLayout_->count() - 1, container);

    JobRow row{id, statusIcon, nameLabel, progressBar, statsLabel, elapsedLabel,
               cancelBtn, retryBtn, container, total, JobStatus::Queued};
    setRowStatus(row, JobStatus::Queued);
    rows_.push_back(row);

    connect(cancelBtn, &QToolButton::clicked, this, [this, id]() {
        manager_->cancelJob(id);
    });
    connect(retryBtn, &QToolButton::clicked, this, [this, id, retryBtn]() {
        if (manager_->retryJob(id))
            retryBtn->hide();
    });

    updateButtonStates();
    updateStatusBar();
}

void TransferProgressWindow::on_jobStarted(int id, const QString& /*name*/, qint64 /*total*/)
{
    auto* row = findRow(id);
    if (!row) return;
    setRowStatus(*row, JobStatus::Running);
    row->statsLabel->setText("Starting…");
    updateStatusBar();
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
                                 .arg(ui::formatBytes(bytes),
                                      ui::formatBytes(total),
                                      ui::formatBytes(static_cast<qint64>(bytesPerSec)),
                                      ui::formatDuration(etaMs)));
    row->elapsedLabel->setText(QStringLiteral("Elapsed: %1").arg(ui::formatDuration(elapsedMs)));
}

void TransferProgressWindow::on_jobFinished(int id, bool success, const QString& error)
{
    auto* row = findRow(id);
    if (!row) return;

    if (success) {
        row->progressBar->setValue(row->progressBar->maximum());
        const QString sizeStr = row->totalBytes > 0 ? ui::formatBytes(row->totalBytes) : QString{};
        row->statsLabel->setText(sizeStr.isEmpty()
                                     ? QStringLiteral("Done")
                                     : QStringLiteral("Done · %1").arg(sizeStr));
        setRowStatus(*row, JobStatus::Completed);
    } else if (error == TransferManager::kCancelledError) {
        row->statsLabel->setText("Cancelled");
        setRowStatus(*row, JobStatus::Cancelled);
    } else {
        row->statsLabel->setText(QStringLiteral("Failed: %1").arg(error));
        setRowStatus(*row, JobStatus::Failed);
    }
    row->elapsedLabel->clear();
    updateButtonStates();
    updateStatusBar();
}

void TransferProgressWindow::on_allFinished()
{
    statusBar_->setText("Idle");
}

void TransferProgressWindow::on_cancelAll_clicked()
{
    manager_->cancelAll();
}

void TransferProgressWindow::on_clearFinished_clicked()
{
    std::vector<QWidget*> keep;
    for (const auto& r : rows_) {
        if (r.status == JobStatus::Queued || r.status == JobStatus::Running)
            keep.push_back(r.container);
        else
            manager_->dismissJob(r.jobId);
    }

    while (jobLayout_->count() > 1) {
        auto* item = jobLayout_->takeAt(0);
        if (auto* w = item->widget()) {
            const bool isKeep = std::find(keep.begin(), keep.end(), w) != keep.end();
            if (!isKeep) w->deleteLater();
        }
        delete item;
    }

    rows_.erase(std::remove_if(rows_.begin(), rows_.end(),
        [](const JobRow& r) {
            return r.status != JobStatus::Queued && r.status != JobStatus::Running;
        }), rows_.end());

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
    updateStatusBar();
    if (autoScrollCheck_->isChecked())
        scrollToActive();
}

void TransferProgressWindow::setRowStatus(JobRow& row, JobStatus status)
{
    row.status = status;

    QStyle::StandardPixmap sp = QStyle::SP_FileIcon;
    switch (status) {
    case JobStatus::Queued:    sp = QStyle::SP_FileIcon;             break;
    case JobStatus::Running:   sp = QStyle::SP_BrowserReload;        break;
    case JobStatus::Completed: sp = QStyle::SP_DialogOkButton;       break;
    case JobStatus::Failed:    sp = QStyle::SP_MessageBoxCritical;   break;
    case JobStatus::Cancelled: sp = QStyle::SP_DialogCancelButton;   break;
    }
    row.statusIcon->setPixmap(style()->standardPixmap(sp).scaled(16, 16, Qt::KeepAspectRatio,
                                                                  Qt::SmoothTransformation));

    const bool inFlight = (status == JobStatus::Queued || status == JobStatus::Running);
    const bool canRetry = (status == JobStatus::Failed || status == JobStatus::Cancelled);
    row.cancelBtn->setVisible(inFlight);
    row.retryBtn->setVisible(canRetry);
}

void TransferProgressWindow::updateButtonStates()
{
    const bool hasAny     = !rows_.empty();
    const bool hasActive  = std::any_of(rows_.begin(), rows_.end(),
                                [](const JobRow& r) {
                                    return r.status == JobStatus::Queued ||
                                           r.status == JobStatus::Running;
                                });
    const bool hasDone    = std::any_of(rows_.begin(), rows_.end(),
                                [](const JobRow& r) {
                                    return r.status != JobStatus::Queued &&
                                           r.status != JobStatus::Running;
                                });
    cancelAllBtn_->setEnabled(hasActive);
    clearBtn_->setEnabled(hasAny && hasDone);
}

void TransferProgressWindow::updateStatusBar()
{
    int queued  = 0;
    int running = 0;
    for (const auto& r : rows_) {
        if (r.status == JobStatus::Queued)  ++queued;
        if (r.status == JobStatus::Running) ++running;
    }

    if (running == 0 && queued == 0) {
        statusBar_->setText("Idle");
    } else {
        QString text;
        if (running > 0)
            text += QStringLiteral("Running: %1").arg(running);
        if (queued > 0) {
            if (!text.isEmpty()) text += "  |  ";
            text += QStringLiteral("Queued: %1").arg(queued);
        }
        statusBar_->setText(text);
    }
}

void TransferProgressWindow::scrollToActive()
{
    // Prefer the last Running row (live progress visible); fall back to the last
    // Queued row (most recently added item) if nothing is running yet.
    const JobRow* best = nullptr;
    for (auto it = rows_.rbegin(); it != rows_.rend(); ++it) {
        if (it->status == JobStatus::Running) {
            best = &*it;
            break;
        }
        if (it->status == JobStatus::Queued && !best)
            best = &*it;
    }
    if (best)
        scrollArea_->ensureWidgetVisible(best->container);
    else
        scrollArea_->verticalScrollBar()->setValue(scrollArea_->verticalScrollBar()->maximum());
}


TransferProgressWindow::JobRow* TransferProgressWindow::findRow(int jobId)
{
    for (auto& r : rows_) {
        if (r.jobId == jobId) return &r;
    }
    return nullptr;
}
