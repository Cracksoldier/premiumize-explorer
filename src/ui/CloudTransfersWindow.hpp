#pragma once
#include "api/ApiTypes.hpp"

#include <QList>
#include <QWidget>

class QLabel;
class QProgressBar;
class QScrollArea;
class QTimer;
class QVBoxLayout;
namespace api { class PremiumizeApi; }

class CloudTransfersWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CloudTransfersWindow(api::PremiumizeApi* api, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void on_refresh_clicked();
    void on_transferListReady(const QList<api::CloudTransferEntry>& entries);

private:
    struct TransferRow {
        QString       id;
        QWidget*      container  = nullptr;
        QLabel*       nameLabel  = nullptr;
        QLabel*       statusLabel = nullptr;
        QProgressBar* progressBar = nullptr;
        QLabel*       statsLabel = nullptr;
    };

    static QString formatBytes(qint64 bytes);
    static QString formatEta(qint64 seconds);
    static QString statusColor(const QString& status);

    api::PremiumizeApi* api_;
    QTimer*             pollTimer_;
    QVBoxLayout*        rowLayout_;
    QLabel*             emptyLabel_;
    QList<TransferRow>  rows_;
};
