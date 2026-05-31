#pragma once
#include "api/ApiTypes.hpp"

#include <QMainWindow>

class FilePane;
class TransferProgressWindow;
class TransferManager;
namespace api { class PremiumizeApi; }

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void on_folderListingReady(const api::FolderListing& listing);
    void on_networkError(const QString& message);
    void on_deleteRequested(const QString& itemId, bool isFolder);
    void on_createFolderRequested(const QString& parentId);
    void on_cloudNavigateRequested(const QString& folderId);
    void on_showTransfers_clicked();
    void on_accountInfoReady(const api::AccountInfo& info);

private:
    void setupLayout();
    void setupMenuBar();
    void connectSignals();
    void loadCloudRoot();
    void loadCloudFolder(const QString& folderId);
    void applyTheme(bool dark);

    api::PremiumizeApi*     api_;
    TransferManager*        transferManager_;
    TransferProgressWindow* progressWindow_;
    FilePane*               localPane_;
    FilePane*               cloudPane_;
};
