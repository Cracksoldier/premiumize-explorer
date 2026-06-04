#pragma once

#include "api/ApiTypes.hpp"

#include <QElapsedTimer>
#include <QList>
#include <QWizard>

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;
class TransferManager;
namespace api { class PremiumizeApi; }

enum BatchWizardPage { PageSearch = 0, PageDestination = 1, PageProgress = 2 };

// ── SearchPage ────────────────────────────────────────────────────────────────

class SearchPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SearchPage(api::PremiumizeApi* api, QWidget* parent = nullptr);

    bool isComplete() const override;
    int  nextId()     const override;

    QList<api::FolderItem> selectedItems() const;

private slots:
    void on_search_clicked();
    void on_searchResultsReady(const QList<api::FolderItem>& items);
    void on_networkError(const QString& message);
    void on_selectionChanged();

private:
    api::PremiumizeApi* api_;
    QLineEdit*          queryEdit_      = nullptr;
    QPushButton*        searchBtn_      = nullptr;
    QListWidget*        resultsList_    = nullptr;
    QPushButton*        selectAllBtn_   = nullptr;
    QPushButton*        deselectAllBtn_ = nullptr;
    QLabel*             statusLabel_    = nullptr;

    QList<api::FolderItem> currentResults_;
    bool                   hasChecked_ = false;
    bool                   searching_  = false;
};

// ── DestinationPage ───────────────────────────────────────────────────────────

class DestinationPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit DestinationPage(QWidget* parent = nullptr);

    void    initializePage()          override;
    bool    isComplete()        const override;
    int     nextId()            const override;

    void    setInitialPath(const QString& path);
    QString destinationPath()   const;

private slots:
    void on_browse_clicked();
    void on_pathChanged(const QString& path);

private:
    QLabel*      summaryLabel_ = nullptr;
    QLineEdit*   pathEdit_     = nullptr;
    QPushButton* browseBtn_    = nullptr;
    bool         pathValid_    = false;
    QString      initialPath_;
};

// ── ProgressPage ──────────────────────────────────────────────────────────────

class ProgressPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit ProgressPage(TransferManager* manager, QWidget* parent = nullptr);

    void initializePage() override;
    bool isComplete()     const override;
    int  nextId()         const override;

    void cancelBatch();

private slots:
    void on_jobStarted(int id, const QString& name, qint64 total);
    void on_jobProgress(int id, qint64 bytes, qint64 total,
                        qint64 elapsedMs, qint64 etaMs, double bytesPerSec);
    void on_jobFinished(int id, bool success, const QString& error);
    void on_cancel_clicked();

private:
    void startNextFile();
    void markDone();
    void updateTimerLabel();

    TransferManager* manager_;

    QLabel*       currentNameLabel_ = nullptr;
    QProgressBar* currentBar_       = nullptr;
    QProgressBar* totalBar_         = nullptr;
    QLabel*       timerLabel_       = nullptr;
    QListWidget*  fileList_         = nullptr;
    QPushButton*  cancelBtn_        = nullptr;
    QTimer*       clockTimer_       = nullptr;

    QList<api::FolderItem> items_;
    QString                destPath_;
    int                    currentIndex_     = 0;
    int                    totalCount_       = 0;
    int                    currentJobId_     = -1;
    qint64                 currentFileEtaMs_ = -1;
    QElapsedTimer          batchTimer_;
    QElapsedTimer          fileTimer_;
    bool                   allDone_     = false;
    bool                   cancelled_   = false;
    bool                   initialized_ = false;
};

// ── Wizard container ──────────────────────────────────────────────────────────

class BatchDownloadWizard : public QWizard
{
    Q_OBJECT
public:
    explicit BatchDownloadWizard(api::PremiumizeApi* api,
                                  TransferManager*    manager,
                                  const QString&      initialLocalPath,
                                  QWidget*            parent = nullptr);

protected:
    void reject() override;

private:
    SearchPage*      searchPage_;
    DestinationPage* destPage_;
    ProgressPage*    progressPage_;
};
