#include "CloudTransfersWindow.hpp"
#include "api/PremiumizeApi.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

static constexpr int kPollIntervalMs = 5000;

CloudTransfersWindow::CloudTransfersWindow(api::PremiumizeApi* api, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::Tool)
    , api_(api)
{
    setWindowTitle("Cloud Transfers");
    setMinimumSize(560, 350);
    resize(600, 420);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scrollArea);
    rowLayout_ = new QVBoxLayout(container);
    rowLayout_->setContentsMargins(0, 0, 0, 0);
    rowLayout_->setSpacing(1);

    emptyLabel_ = new QLabel("No active transfers.", container);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    rowLayout_->addWidget(emptyLabel_);
    rowLayout_->addStretch();

    scrollArea->setWidget(container);
    mainLayout->addWidget(scrollArea);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 0, 0, 0);
    auto* refreshBtn = new QPushButton("Refresh", this);
    btnRow->addWidget(refreshBtn);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(kPollIntervalMs);
    pollTimer_->setSingleShot(false);

    connect(refreshBtn, &QPushButton::clicked, this, &CloudTransfersWindow::on_refresh_clicked);
    connect(pollTimer_, &QTimer::timeout, api_, &api::PremiumizeApi::fetchTransferList);
    connect(api_, &api::PremiumizeApi::transferListReady,
            this, &CloudTransfersWindow::on_transferListReady);
}

void CloudTransfersWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    api_->fetchTransferList();
    pollTimer_->start();
}

void CloudTransfersWindow::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    pollTimer_->stop();
}

void CloudTransfersWindow::on_refresh_clicked()
{
    api_->fetchTransferList();
}

void CloudTransfersWindow::on_transferListReady(const QList<api::CloudTransferEntry>& entries)
{
    // Remove old rows from layout
    for (auto& row : rows_)
        delete row.container;
    rows_.clear();

    const bool hasEntries = !entries.isEmpty();
    emptyLabel_->setVisible(!hasEntries);

    for (const auto& entry : entries) {
        TransferRow row;
        row.id = entry.id;

        row.container = new QWidget;
        auto* vl = new QVBoxLayout(row.container);
        vl->setContentsMargins(6, 6, 6, 6);
        vl->setSpacing(3);

        // Name + status on one line
        auto* topRow = new QHBoxLayout;
        topRow->setContentsMargins(0, 0, 0, 0);
        topRow->setSpacing(8);

        row.nameLabel = new QLabel(entry.name, row.container);
        QFont f = row.nameLabel->font();
        f.setBold(true);
        row.nameLabel->setFont(f);
        row.nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        row.statusLabel = new QLabel(row.container);

        topRow->addWidget(row.nameLabel);
        topRow->addWidget(row.statusLabel);
        vl->addLayout(topRow);

        row.progressBar = new QProgressBar(row.container);
        row.progressBar->setRange(0, 100);
        row.progressBar->setTextVisible(true);
        vl->addWidget(row.progressBar);

        row.statsLabel = new QLabel(row.container);
        row.statsLabel->setEnabled(false);
        vl->addWidget(row.statsLabel);

        // Populate values
        const QString color = statusColor(entry.status);
        row.statusLabel->setText(
            QStringLiteral("<span style='color:%1'>%2</span>").arg(color, entry.status));

        const int pct = qBound(0, static_cast<int>(entry.progress * 100.f), 100);
        row.progressBar->setValue(pct);

        const bool isActive = (entry.status == "running" || entry.status == "seeding");
        row.progressBar->setVisible(isActive || (pct > 0 && pct < 100));

        if (isActive && entry.speedDown > 0) {
            QString stats = QStringLiteral("↓ %1/s").arg(formatBytes(entry.speedDown));
            if (entry.eta >= 0)
                stats += QStringLiteral("  —  ETA %1").arg(formatEta(entry.eta));
            row.statsLabel->setText(stats);
            row.statsLabel->setVisible(true);
        } else if (!entry.message.isEmpty()) {
            row.statsLabel->setText(entry.message);
            row.statsLabel->setVisible(true);
        } else {
            row.statsLabel->setVisible(false);
        }

        // Separator line above each row except first
        if (!rows_.isEmpty()) {
            auto* sep = new QFrame(row.container->parentWidget());
            sep->setFrameShape(QFrame::HLine);
            sep->setFrameShadow(QFrame::Sunken);
            rowLayout_->insertWidget(rowLayout_->count() - 1, sep);
        }

        rowLayout_->insertWidget(rowLayout_->count() - 1, row.container);
        rows_.append(std::move(row));
    }
}

QString CloudTransfersWindow::formatBytes(qint64 bytes)
{
    if (bytes < 0) return "?";
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024);
    if (bytes < 1024 * 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / (1024 * 1024));
    return QStringLiteral("%1 GB").arg(bytes / (1024 * 1024 * 1024));
}

QString CloudTransfersWindow::formatEta(qint64 seconds)
{
    if (seconds < 0) return "?";
    if (seconds >= 60) return QStringLiteral("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    return QStringLiteral("%1s").arg(seconds);
}

QString CloudTransfersWindow::statusColor(const QString& status)
{
    if (status == "running")  return "#4a9eff";
    if (status == "seeding")  return "#4a9eff";
    if (status == "finished") return "#4caf50";
    if (status == "error")    return "#f44336";
    if (status == "deleted")  return "#f44336";
    return "#888888"; // waiting, unknown
}
