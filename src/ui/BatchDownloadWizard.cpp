#include "BatchDownloadWizard.hpp"
#include "FormatHelpers.hpp"
#include "api/PremiumizeApi.hpp"
#include "transfer/TransferManager.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

// ── SearchPage ────────────────────────────────────────────────────────────────

SearchPage::SearchPage(api::PremiumizeApi* api, QWidget* parent)
    : QWizardPage(parent)
    , api_(api)
{
    setTitle("Search Cloud Files");
    setSubTitle("Enter keywords to search for files in your Premiumize cloud storage.");

    auto* vl = new QVBoxLayout(this);

    auto* searchRow = new QHBoxLayout;
    queryEdit_ = new QLineEdit;
    queryEdit_->setPlaceholderText("Search keywords…");
    searchBtn_ = new QPushButton("Search");
    searchBtn_->setEnabled(false);
    searchRow->addWidget(queryEdit_);
    searchRow->addWidget(searchBtn_);
    vl->addLayout(searchRow);

    resultsList_ = new QListWidget;
    vl->addWidget(resultsList_, 1);

    auto* selRow = new QHBoxLayout;
    selectAllBtn_   = new QPushButton("Select All");
    deselectAllBtn_ = new QPushButton("Deselect All");
    statusLabel_    = new QLabel;
    selectAllBtn_->setEnabled(false);
    deselectAllBtn_->setEnabled(false);
    selRow->addWidget(selectAllBtn_);
    selRow->addWidget(deselectAllBtn_);
    selRow->addStretch();
    selRow->addWidget(statusLabel_);
    vl->addLayout(selRow);

    connect(queryEdit_, &QLineEdit::textChanged, this, [this](const QString& t) {
        searchBtn_->setEnabled(!t.trimmed().isEmpty());
    });
    connect(queryEdit_, &QLineEdit::returnPressed, this, &SearchPage::on_search_clicked);
    connect(searchBtn_, &QPushButton::clicked,     this, &SearchPage::on_search_clicked);

    connect(selectAllBtn_, &QPushButton::clicked, this, [this]() {
        resultsList_->blockSignals(true);
        for (int i = 0; i < resultsList_->count(); ++i)
            resultsList_->item(i)->setCheckState(Qt::Checked);
        resultsList_->blockSignals(false);
        on_selectionChanged();
    });
    connect(deselectAllBtn_, &QPushButton::clicked, this, [this]() {
        resultsList_->blockSignals(true);
        for (int i = 0; i < resultsList_->count(); ++i)
            resultsList_->item(i)->setCheckState(Qt::Unchecked);
        resultsList_->blockSignals(false);
        on_selectionChanged();
    });
    connect(resultsList_, &QListWidget::itemChanged,
            this, &SearchPage::on_selectionChanged);

    connect(api_, &api::PremiumizeApi::searchResultsReady,
            this, &SearchPage::on_searchResultsReady);
    connect(api_, &api::PremiumizeApi::networkError,
            this, &SearchPage::on_networkError);
}

bool SearchPage::isComplete() const
{
    return hasChecked_;
}

int SearchPage::nextId() const
{
    return PageDestination;
}

QList<api::FolderItem> SearchPage::selectedItems() const
{
    QList<api::FolderItem> result;
    for (int i = 0; i < resultsList_->count(); ++i) {
        if (resultsList_->item(i)->checkState() == Qt::Checked && i < currentResults_.size()) {
            const auto& item = currentResults_[i];
            if (item.link.has_value() && !item.link->isEmpty())
                result.append(item);
        }
    }
    return result;
}

void SearchPage::on_search_clicked()
{
    const QString query = queryEdit_->text().trimmed();
    if (query.isEmpty()) return;

    searching_ = true;
    searchBtn_->setEnabled(false);
    resultsList_->clear();
    currentResults_.clear();
    selectAllBtn_->setEnabled(false);
    deselectAllBtn_->setEnabled(false);
    statusLabel_->setText("Searching…");
    hasChecked_ = false;
    emit completeChanged();

    api_->searchItems(query);
}

void SearchPage::on_searchResultsReady(const QList<api::FolderItem>& items)
{
    if (!searching_) return;
    searching_ = false;
    searchBtn_->setEnabled(!queryEdit_->text().trimmed().isEmpty());

    resultsList_->blockSignals(true);
    resultsList_->clear();
    currentResults_.clear();

    for (const auto& item : items) {
        currentResults_.append(item);
        const QString sizeStr = item.size.has_value()
            ? QStringLiteral("  (%1)").arg(ui::formatBytes(*item.size))
            : QString{};
        auto* listItem = new QListWidgetItem(item.name + sizeStr);
        listItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        listItem->setCheckState(Qt::Unchecked);
        resultsList_->addItem(listItem);
    }
    resultsList_->blockSignals(false);

    const bool hasResults = !items.isEmpty();
    selectAllBtn_->setEnabled(hasResults);
    deselectAllBtn_->setEnabled(hasResults);
    statusLabel_->setText(hasResults
        ? QStringLiteral("%1 file(s) found").arg(items.size())
        : "No results found");

    on_selectionChanged();
}

void SearchPage::on_networkError(const QString& message)
{
    if (!searching_) return;
    searching_ = false;
    searchBtn_->setEnabled(!queryEdit_->text().trimmed().isEmpty());
    statusLabel_->setText(QStringLiteral("Search failed: %1").arg(message));
}

void SearchPage::on_selectionChanged()
{
    int    checked   = 0;
    qint64 totalSize = 0;
    bool   allHaveSize = true;

    for (int i = 0; i < resultsList_->count(); ++i) {
        if (resultsList_->item(i)->checkState() == Qt::Checked) {
            ++checked;
            if (i < currentResults_.size() && currentResults_[i].size.has_value())
                totalSize += *currentResults_[i].size;
            else
                allHaveSize = false;
        }
    }

    hasChecked_ = (checked > 0);

    if (checked > 0) {
        statusLabel_->setText(allHaveSize
            ? QStringLiteral("%1 selected · %2").arg(checked).arg(ui::formatBytes(totalSize))
            : QStringLiteral("%1 selected").arg(checked));
    } else if (!currentResults_.isEmpty()) {
        statusLabel_->setText(QStringLiteral("%1 file(s) found").arg(currentResults_.size()));
    }

    emit completeChanged();
}

// ── DestinationPage ───────────────────────────────────────────────────────────

DestinationPage::DestinationPage(QWidget* parent)
    : QWizardPage(parent)
{
    setTitle("Choose Download Location");
    setSubTitle("Select the folder where files will be saved.");

    auto* vl = new QVBoxLayout(this);

    summaryLabel_ = new QLabel;
    vl->addWidget(summaryLabel_);

    auto* pathRow = new QHBoxLayout;
    pathEdit_ = new QLineEdit;
    browseBtn_ = new QPushButton("Browse…");
    pathRow->addWidget(pathEdit_);
    pathRow->addWidget(browseBtn_);
    vl->addLayout(pathRow);

    vl->addStretch();

    connect(pathEdit_,  &QLineEdit::textChanged,    this, &DestinationPage::on_pathChanged);
    connect(browseBtn_, &QPushButton::clicked,       this, &DestinationPage::on_browse_clicked);
}

void DestinationPage::setInitialPath(const QString& path)
{
    initialPath_ = path;
}

void DestinationPage::initializePage()
{
    auto* sp = qobject_cast<SearchPage*>(wizard()->page(PageSearch));
    if (sp) {
        const auto items = sp->selectedItems();
        qint64 totalSize  = 0;
        bool   allHaveSize = true;
        for (const auto& item : items) {
            if (item.size.has_value()) totalSize += *item.size;
            else allHaveSize = false;
        }
        summaryLabel_->setText(allHaveSize
            ? QStringLiteral("%1 file(s) · %2 total")
                  .arg(items.size()).arg(ui::formatBytes(totalSize))
            : QStringLiteral("%1 file(s) selected").arg(items.size()));
    }

    if (pathEdit_->text().isEmpty())
        pathEdit_->setText(initialPath_);
}

QString DestinationPage::destinationPath() const
{
    return pathEdit_->text();
}

bool DestinationPage::isComplete() const
{
    return pathValid_;
}

int DestinationPage::nextId() const
{
    return PageProgress;
}

void DestinationPage::on_browse_clicked()
{
    const QString chosen = QFileDialog::getExistingDirectory(
        this, "Choose Download Folder", pathEdit_->text());
    if (!chosen.isEmpty())
        pathEdit_->setText(chosen);
}

void DestinationPage::on_pathChanged(const QString& path)
{
    const QFileInfo fi(path);
    pathValid_ = fi.exists() && fi.isDir() && fi.isWritable();
    emit completeChanged();
}

// ── ProgressPage ──────────────────────────────────────────────────────────────

ProgressPage::ProgressPage(TransferManager* manager, QWidget* parent)
    : QWizardPage(parent)
    , manager_(manager)
{
    setTitle("Downloading");
    setSubTitle("Files are being downloaded to the chosen location.");

    auto* vl = new QVBoxLayout(this);

    currentNameLabel_ = new QLabel("Preparing…");
    vl->addWidget(currentNameLabel_);

    vl->addWidget(new QLabel("Current file:"));
    currentBar_ = new QProgressBar;
    currentBar_->setRange(0, 1000);
    currentBar_->setValue(0);
    vl->addWidget(currentBar_);

    vl->addWidget(new QLabel("Overall:"));
    totalBar_ = new QProgressBar;
    totalBar_->setRange(0, 1);
    totalBar_->setValue(0);
    vl->addWidget(totalBar_);

    timerLabel_ = new QLabel("Total: 0s  |  Current file: 0s  |  ETA: ?");
    vl->addWidget(timerLabel_);

    vl->addStretch();

    cancelBtn_ = new QPushButton("Cancel");
    vl->addWidget(cancelBtn_, 0, Qt::AlignRight);

    clockTimer_ = new QTimer(this);
    clockTimer_->setInterval(500);

    connect(clockTimer_, &QTimer::timeout,        this, &ProgressPage::updateTimerLabel);
    connect(cancelBtn_,  &QPushButton::clicked,   this, &ProgressPage::on_cancel_clicked);

    connect(manager_, &TransferManager::jobStarted,  this, &ProgressPage::on_jobStarted);
    connect(manager_, &TransferManager::jobProgress, this, &ProgressPage::on_jobProgress);
    connect(manager_, &TransferManager::jobFinished, this, &ProgressPage::on_jobFinished);
}

void ProgressPage::initializePage()
{
    if (initialized_) return;
    initialized_ = true;

    auto* sp = qobject_cast<SearchPage*>(wizard()->page(PageSearch));
    auto* dp = qobject_cast<DestinationPage*>(wizard()->page(PageDestination));
    if (sp) items_    = sp->selectedItems();
    if (dp) destPath_ = dp->destinationPath();

    totalCount_ = items_.size();
    totalBar_->setRange(0, totalCount_);
    totalBar_->setValue(0);

    wizard()->button(QWizard::BackButton)->setVisible(false);

    batchTimer_.start();
    clockTimer_->start();

    startNextFile();
}

bool ProgressPage::isComplete() const
{
    return allDone_;
}

int ProgressPage::nextId() const
{
    return -1;
}

void ProgressPage::startNextFile()
{
    while (currentIndex_ < totalCount_) {
        const auto& item = items_[currentIndex_];
        if (!item.link.has_value() || item.link->isEmpty()) {
            ++currentIndex_;
            totalBar_->setValue(currentIndex_);
            continue;
        }
        currentBar_->setMaximum(1000);
        currentBar_->setValue(0);
        currentNameLabel_->setText(item.name);

        const QString dest = destPath_ + QDir::separator() + item.name;
        // cancelAll() is safe here: wizard is modal so no other downloads are in-flight
        manager_->enqueueDownload(*item.link, dest, item.size.value_or(-1), item.name);
        return;
    }
    markDone();
}

void ProgressPage::on_jobStarted(int id, const QString& /*name*/, qint64 total)
{
    currentJobId_ = id;
    fileTimer_.restart();
    currentFileEtaMs_ = -1;
    currentBar_->setValue(0);
    currentBar_->setMaximum(total > 0 ? 1000 : 0);
}

void ProgressPage::on_jobProgress(int id, qint64 bytes, qint64 total,
                                    qint64 /*elapsedMs*/, qint64 etaMs, double /*bps*/)
{
    if (id != currentJobId_) return;

    currentFileEtaMs_ = etaMs;

    if (total > 0) {
        if (currentBar_->maximum() == 0) currentBar_->setMaximum(1000);
        currentBar_->setValue(static_cast<int>(bytes * 1000 / total));
    }

    updateTimerLabel();
}

void ProgressPage::on_jobFinished(int id, bool success, const QString& error)
{
    if (id != currentJobId_) return;

    if (!success && !cancelled_) {
        const QString name = currentIndex_ < totalCount_ ? items_[currentIndex_].name : QString{};
        currentNameLabel_->setText(
            QStringLiteral("Failed: %1 (%2)").arg(name, error));
    }

    ++currentIndex_;
    totalBar_->setValue(currentIndex_);

    if (!cancelled_)
        startNextFile();
}

void ProgressPage::updateTimerLabel()
{
    const qint64 totalElapsed = batchTimer_.isValid() ? batchTimer_.elapsed() : 0;
    const qint64 fileElapsed  = fileTimer_.isValid()  ? fileTimer_.elapsed()  : 0;
    timerLabel_->setText(
        QStringLiteral("Total: %1  |  Current file: %2  |  ETA: %3")
            .arg(ui::formatDuration(totalElapsed),
                 ui::formatDuration(fileElapsed),
                 ui::formatDuration(currentFileEtaMs_)));
}

void ProgressPage::markDone()
{
    allDone_ = true;
    clockTimer_->stop();
    cancelBtn_->setVisible(false);
    currentNameLabel_->setText("All downloads complete.");
    currentBar_->setMaximum(1000);
    currentBar_->setValue(1000);
    totalBar_->setValue(totalCount_);
    updateTimerLabel();
    emit completeChanged();
}

void ProgressPage::on_cancel_clicked()
{
    cancelled_ = true;
    // cancelAll() is safe here: wizard is modal so no other downloads are in-flight
    manager_->cancelAll();
    allDone_ = true;
    clockTimer_->stop();
    cancelBtn_->setVisible(false);
    currentNameLabel_->setText("Cancelled.");
    emit completeChanged();
}

// ── Wizard container ──────────────────────────────────────────────────────────

BatchDownloadWizard::BatchDownloadWizard(api::PremiumizeApi* api,
                                          TransferManager*    manager,
                                          const QString&      initialLocalPath,
                                          QWidget*            parent)
    : QWizard(parent)
{
    setWindowTitle("Batch Download");
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoCancelButton, true);
    setButtonText(QWizard::FinishButton, "Close");

    searchPage_   = new SearchPage(api, this);
    destPage_     = new DestinationPage(this);
    progressPage_ = new ProgressPage(manager, this);

    destPage_->setInitialPath(initialLocalPath);

    setPage(PageSearch,      searchPage_);
    setPage(PageDestination, destPage_);
    setPage(PageProgress,    progressPage_);
    setStartId(PageSearch);

    resize(620, 520);
}
