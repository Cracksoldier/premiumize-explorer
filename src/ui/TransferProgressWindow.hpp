#pragma once
#include <QWidget>
#include <QString>
#include <vector>

class QProgressBar;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QScrollArea;
class TransferManager;

class TransferProgressWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TransferProgressWindow(TransferManager* manager, QWidget* parent = nullptr);

private slots:
    void on_jobStarted(int id, const QString& name, qint64 total);
    void on_jobProgress(int id, qint64 bytes, qint64 total,
                        qint64 elapsedMs, qint64 etaMs, double bytesPerSec);
    void on_jobFinished(int id, bool success, const QString& error);
    void on_cancelAll_clicked();

private:
    static QString formatBytes(qint64 bytes);
    static QString formatDuration(qint64 ms);

    struct JobRow {
        int           jobId;
        QLabel*       nameLabel;
        QProgressBar* progressBar;
        QLabel*       statsLabel;
        QLabel*       elapsedLabel;
        QWidget*      container;
    };

    JobRow* findRow(int jobId);

    QVBoxLayout*        jobLayout_;
    QPushButton*        cancelAllBtn_;
    TransferManager*    manager_;
    std::vector<JobRow> rows_;
};
