#pragma once
#include <QWidget>
#include <QString>
#include <vector>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;
class TransferManager;

class TransferProgressWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TransferProgressWindow(TransferManager* manager, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_jobQueued(int id, const QString& name, qint64 total);
    void on_jobStarted(int id, const QString& name, qint64 total);
    void on_jobProgress(int id, qint64 bytes, qint64 total,
                        qint64 elapsedMs, qint64 etaMs, double bytesPerSec);
    void on_jobFinished(int id, bool success, const QString& error);
    void on_allFinished();
    void on_cancelAll_clicked();
    void on_clearFinished_clicked();

private:
    enum class JobStatus { Queued, Running, Completed, Failed, Cancelled };

    struct JobRow {
        int           jobId;
        QLabel*       statusIcon;
        QLabel*       nameLabel;
        QProgressBar* progressBar;
        QLabel*       statsLabel;
        QLabel*       elapsedLabel;
        QToolButton*  cancelBtn;
        QToolButton*  retryBtn;
        QWidget*      container;
        qint64        totalBytes;
        JobStatus     status;
    };

    JobRow* findRow(int jobId);
    void    setRowStatus(JobRow& row, JobStatus status);
    void    updateButtonStates();
    void    updateStatusBar();
    void    scrollToActive();

    QScrollArea*        scrollArea_;
    QVBoxLayout*        jobLayout_;
    QPushButton*        cancelAllBtn_;
    QToolButton*        clearBtn_;
    QLabel*             statusBar_;
    QCheckBox*          autoScrollCheck_;
    QCheckBox*          stayOnTopCheck_;
    TransferManager*    manager_;
    std::vector<JobRow> rows_;
};
