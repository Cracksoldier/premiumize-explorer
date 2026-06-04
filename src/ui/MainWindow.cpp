#include "MainWindow.hpp"
#include "ApiKeyDialog.hpp"
#include "CloudTransfersWindow.hpp"
#include "CreateFolderDialog.hpp"
#include "FilePane.hpp"
#include "LogWindow.hpp"
#include "BatchDownloadWizard.hpp"
#include "TransferProgressWindow.hpp"
#include "api/PremiumizeApi.hpp"
#include "config/AppConfig.hpp"
#include "model/PremiumizeModel.hpp"
#include "transfer/TransferManager.hpp"

#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    applyTheme(AppConfig::instance().darkModeEnabled());

    setWindowTitle("Premiumize Explorer");
    resize(1100, 650);

    api_            = new api::PremiumizeApi(this);
    api_->setApiKey(AppConfig::instance().apiKey());

    transferManager_      = new TransferManager(api_, this);
    progressWindow_       = new TransferProgressWindow(transferManager_, this);
    logWindow_            = new LogWindow(this);
    cloudTransfersWindow_ = new CloudTransfersWindow(api_, this);

    setupLayout();
    setupMenuBar();
    connectSignals();

    const auto geom = AppConfig::instance().windowGeometry();
    if (!geom.isEmpty()) restoreGeometry(geom);

    loadCloudRoot();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupLayout()
{
    auto* central   = new QWidget(this);
    auto* vl        = new QVBoxLayout(central);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* splitter  = new QSplitter(Qt::Horizontal, central);

    localPane_ = new FilePane(FilePane::PaneType::Local,  splitter);
    cloudPane_ = new FilePane(FilePane::PaneType::Cloud,  splitter);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({540, 540});

    const auto splitterState = AppConfig::instance().splitterSizes();
    if (!splitterState.isEmpty()) splitter->restoreState(splitterState);

    connect(splitter, &QSplitter::splitterMoved, this, [splitter]() {
        AppConfig::instance().setSplitterSizes(splitter->saveState());
    });

    vl->addWidget(splitter);
    setCentralWidget(central);

    localPane_->setLocalPath(AppConfig::instance().lastLocalPath());
    cloudPane_->setDownloadPath(AppConfig::instance().lastLocalPath());

    statusBar()->showMessage("Connecting…");
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* changeKeyAct = fileMenu->addAction("Change API Key…");
    connect(changeKeyAct, &QAction::triggered, this, [this]() {
        ApiKeyDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            AppConfig::instance().setApiKey(dlg.apiKey());
            api_->setApiKey(dlg.apiKey());
            loadCloudRoot();
        }
    });

    auto* batchDownloadAct = fileMenu->addAction("Batch Download…");
    batchDownloadAct->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(batchDownloadAct, &QAction::triggered,
            this, &MainWindow::on_batchDownload_clicked);

    fileMenu->addSeparator();

    auto* quitAct = fileMenu->addAction("&Quit");
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu("&View");
    auto* showTransfersAct = viewMenu->addAction("&Transfers");
    connect(showTransfersAct, &QAction::triggered, this, &MainWindow::on_showTransfers_clicked);

    auto* showCloudTransfersAct = viewMenu->addAction("&Cloud Transfers");
    connect(showCloudTransfersAct, &QAction::triggered,
            this, &MainWindow::on_showCloudTransfers_clicked);

    viewMenu->addSeparator();

    auto* darkAct = viewMenu->addAction("Dark Mode");
    darkAct->setCheckable(true);
    darkAct->setChecked(AppConfig::instance().darkModeEnabled());
    connect(darkAct, &QAction::toggled, this, [this](bool on) {
        applyTheme(on);
    });

    viewMenu->addSeparator();

    auto* showApiLogAct = viewMenu->addAction("API &Log");
    connect(showApiLogAct, &QAction::triggered, this, &MainWindow::on_showApiLog_clicked);
}

void MainWindow::connectSignals()
{
    // API signals
    connect(api_, &api::PremiumizeApi::folderListingReady,
            this, &MainWindow::on_folderListingReady);
    connect(api_, &api::PremiumizeApi::networkError,
            this, &MainWindow::on_networkError);
    connect(api_, &api::PremiumizeApi::accountInfoReady,
            this, &MainWindow::on_accountInfoReady);
    connect(api_, &api::PremiumizeApi::folderCreated,
            this, [this](const QString& /*id*/) {
                loadCloudFolder(cloudPane_->currentCloudId());
            });
    connect(api_, &api::PremiumizeApi::deleteFinished,
            this, [this](bool success, const QString& err) {
                if (!success) {
                    QMessageBox::warning(this, "Delete failed", err);
                }
                loadCloudFolder(cloudPane_->currentCloudId());
            });
    connect(api_, &api::PremiumizeApi::requestLogged,
            logWindow_, &LogWindow::appendEntry);

    // Local pane signals
    connect(localPane_, &FilePane::localPathChanged, this, [this](const QString& path) {
        AppConfig::instance().setLastLocalPath(path);
        cloudPane_->setDownloadPath(path);
    });

    // Cloud pane signals
    connect(cloudPane_, &FilePane::navigateCloudRequested,
            this, &MainWindow::on_cloudNavigateRequested);
    connect(cloudPane_, &FilePane::refreshRequested,
            this, [this]() { loadCloudFolder(cloudPane_->currentCloudId()); });
    connect(cloudPane_, &FilePane::createFolderRequested,
            this, &MainWindow::on_createFolderRequested);
    connect(cloudPane_, &FilePane::deleteRequested,
            this, &MainWindow::on_deleteRequested);
    connect(cloudPane_, &FilePane::downloadRequested,
            this, [this](const QString& url, const QString& dest,
                         qint64 size, const QString& name) {
                transferManager_->enqueueDownload(url, dest, size, name);
                progressWindow_->show();
                progressWindow_->raise();
            });
    connect(cloudPane_, &FilePane::folderDownloadRequested,
            this, [this](const QString& id, const QString& name, const QString& dest) {
                if (pendingFolderDownloads_.contains(id)) return;
                pendingFolderDownloads_.insert(id, {name, dest});
                api_->fetchFolderDownloadLink(id);
            });
    connect(api_, &api::PremiumizeApi::folderDownloadLinkReady,
            this, [this](const QString& id, const QString& url) {
                if (!pendingFolderDownloads_.contains(id)) return;
                const auto [name, dest] = pendingFolderDownloads_.take(id);
                if (url.isEmpty()) {
                    statusBar()->showMessage(
                        QStringLiteral("Download link unavailable for \"%1\"").arg(name));
                    return;
                }
                transferManager_->enqueueDownload(url, dest, -1, name);
                progressWindow_->show();
                progressWindow_->raise();
            });
    connect(cloudPane_, &FilePane::uploadRequested,
            this, [this](const QStringList& paths, const QString& folderId) {
                transferManager_->enqueueUpload(paths, folderId);
                progressWindow_->show();
                progressWindow_->raise();
            });

    // Local pane upload via drag to cloud — handled via cloudPane drop
    connect(localPane_, &FilePane::uploadRequested,
            this, [this](const QStringList& paths, const QString& folderId) {
                transferManager_->enqueueUpload(paths, folderId);
                progressWindow_->show();
                progressWindow_->raise();
            });

    connect(transferManager_, &TransferManager::uploadFinished,
            this, [this](const QString& folderId, bool success, const QString&) {
                if (success && folderId == cloudPane_->currentCloudId()) {
                    loadCloudFolder(folderId);
                }
            });
}

void MainWindow::loadCloudRoot()
{
    api_->fetchAccountInfo();
    loadCloudFolder({});
}

void MainWindow::loadCloudFolder(const QString& folderId)
{
    statusBar()->showMessage("Loading…");
    api_->listFolder(folderId);
}

void MainWindow::on_folderListingReady(const api::FolderListing& listing)
{
    cloudPane_->setCloudListing(listing.folderId, listing.name, listing.parentId);
    localPane_->setUploadTargetFolderId(listing.folderId);
    cloudPane_->cloudModel()->populate(listing);
    statusBar()->showMessage(
        QStringLiteral("%1 item(s)").arg(listing.content.size()));
}

void MainWindow::on_networkError(const QString& message)
{
    statusBar()->showMessage(QStringLiteral("Error: %1").arg(message));
}

void MainWindow::on_deleteRequested(const QString& itemId, bool isFolder)
{
    api_->deleteItem(itemId, isFolder);
}

void MainWindow::on_createFolderRequested(const QString& parentId)
{
    CreateFolderDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        api_->createFolder(dlg.folderName(), parentId);
    }
}

void MainWindow::on_cloudNavigateRequested(const QString& folderId)
{
    loadCloudFolder(folderId);
}

void MainWindow::on_showTransfers_clicked()
{
    progressWindow_->show();
    progressWindow_->raise();
    progressWindow_->activateWindow();
}

void MainWindow::on_showApiLog_clicked()
{
    logWindow_->show();
    logWindow_->raise();
    logWindow_->activateWindow();
}

void MainWindow::on_showCloudTransfers_clicked()
{
    cloudTransfersWindow_->show();
    cloudTransfersWindow_->raise();
    cloudTransfersWindow_->activateWindow();
}

void MainWindow::on_batchDownload_clicked()
{
    BatchDownloadWizard wizard(api_, transferManager_,
                                localPane_->currentLocalPath(), this);
    wizard.exec();
}

void MainWindow::on_accountInfoReady(const api::AccountInfo& info)
{
    const auto used  = info.spaceUsed  / (1LL << 30);
    const auto total = info.spaceTotal / (1LL << 30);
    statusBar()->showMessage(
        QStringLiteral("Cloud: %1 GB used / %2 GB total").arg(used).arg(total));
}

void MainWindow::applyTheme(bool dark)
{
    auto* qlStyle = qobject_cast<oclero::qlementine::QlementineStyle*>(QApplication::style());
    if (!qlStyle) return;
    if (dark) {
        QFile f(":/themes/dark.json");
        if (f.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(f.readAll());
            if (auto t = oclero::qlementine::Theme::fromJsonDoc(doc)) {
                qlStyle->setTheme(*t);
                AppConfig::instance().setDarkModeEnabled(true);
            }
        }
    } else {
        qlStyle->setTheme(oclero::qlementine::Theme{});
        AppConfig::instance().setDarkModeEnabled(false);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    AppConfig::instance().setWindowGeometry(saveGeometry());
    QMainWindow::closeEvent(event);
}
