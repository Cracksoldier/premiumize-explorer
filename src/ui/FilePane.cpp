#include "FilePane.hpp"
#include "model/PremiumizeModel.hpp"
#include "model/UpEntryProxyModel.hpp"

#include <QAction>
#include <QDragEnterEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

static constexpr const char* kCloudMime = "application/x-premiumize-items";

// File-local proxy model: wraps PremiumizeModel and filters by name wildcard.
// The "↑ Up" virtual row (source row 0 when present) always passes through.
class CloudFilterProxy : public QSortFilterProxyModel
{
public:
    explicit CloudFilterProxy(PremiumizeModel* src, QObject* parent = nullptr)
        : QSortFilterProxyModel(parent), source_(src)
    {
        setSourceModel(src);
    }

    void setPattern(const QString& raw) {
        beginFilterChange();
        pattern_ = raw;
        QString wild = raw;
        if (!wild.isEmpty() && !wild.contains('*') && !wild.contains('?'))
            wild = '*' + wild + '*';
        re_ = QRegularExpression(
            QRegularExpression::wildcardToRegularExpression(wild),
            QRegularExpression::CaseInsensitiveOption);
        endFilterChange();
    }

    // Helpers: take proxy-model indices and delegate to source model.
    bool isUpEntryAt(const QModelIndex& proxyIdx) const {
        return source_->isUpEntry(mapToSource(proxyIdx).row());
    }
    const api::FolderItem* itemAt(const QModelIndex& proxyIdx) const {
        return source_->itemAtViewRow(mapToSource(proxyIdx).row());
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex&) const override {
        if (source_->isUpEntry(sourceRow)) return true;
        if (pattern_.isEmpty()) return true;
        const auto* item = source_->itemAtViewRow(sourceRow);
        return item && re_.match(item->name).hasMatch();
    }

private:
    PremiumizeModel*   source_;
    QRegularExpression re_;
    QString            pattern_;
};

FilePane::FilePane(PaneType type, QWidget* parent)
    : QWidget(parent)
    , type_(type)
{
    setupLayout();
    if (type_ == PaneType::Local) {
        setupLocalModel();
    } else {
        setupCloudModel();
    }
    setAcceptDrops(true);
}

void FilePane::setupLayout()
{
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);

    auto* toolbar = new QToolBar(this);
    toolbar->setIconSize({16, 16});

    if (type_ == PaneType::Cloud) {
        auto* refreshAct = toolbar->addAction("⟳");
        connect(refreshAct, &QAction::triggered, this, &FilePane::refreshRequested);

        auto* newFolderAct = toolbar->addAction("+ Folder");
        connect(newFolderAct, &QAction::triggered, this, &FilePane::on_createFolder_clicked);

        auto* deleteAct = toolbar->addAction("Delete");
        connect(deleteAct, &QAction::triggered, this, &FilePane::on_delete_clicked);

        toolbar->addSeparator();
        filterEdit_ = new QLineEdit(this);
        filterEdit_->setPlaceholderText("Filter…");
        filterEdit_->setClearButtonEnabled(true);
        filterEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        filterEdit_->setMaximumWidth(200);
        toolbar->addWidget(filterEdit_);
    }

    if (type_ == PaneType::Local) {
        auto* upAct = toolbar->addAction("↑ Up");
        connect(upAct, &QAction::triggered, this, &FilePane::on_upButton_clicked);
    }

    vl->addWidget(toolbar);

    pathLabel_ = new QLabel(this);
    pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel_->setContentsMargins(4, 1, 4, 1);
    vl->addWidget(pathLabel_);

    view_ = new QListView(this);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setDragEnabled(true);
    view_->setDragDropMode(QAbstractItemView::DragOnly);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QListView::doubleClicked,          this, &FilePane::on_itemActivated);
    connect(view_, &QListView::customContextMenuRequested, this, &FilePane::on_contextMenu);

    vl->addWidget(view_);
}

void FilePane::setupLocalModel()
{
    localModel_ = new QFileSystemModel(this);
    localModel_->setRootPath({});
    localModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    upProxy_ = new UpEntryProxyModel(this);
    upProxy_->setSourceModel(localModel_);
    view_->setModel(upProxy_);
}

void FilePane::setupCloudModel()
{
    cloudModel_       = new PremiumizeModel(this);
    cloudFilterProxy_ = new CloudFilterProxy(cloudModel_, this);
    view_->setModel(cloudFilterProxy_);

    connect(filterEdit_, &QLineEdit::textChanged,
            cloudFilterProxy_, &CloudFilterProxy::setPattern);
}

void FilePane::setLocalPath(const QString& path)
{
    if (type_ != PaneType::Local || !localModel_) return;
    currentLocalPath_ = path;
    const auto srcIdx   = localModel_->setRootPath(path);
    const auto proxyIdx = upProxy_->setSourceRoot(srcIdx, !QDir(path).isRoot());
    view_->setRootIndex(proxyIdx);
    updatePathLabel();
    emit localPathChanged(path);
}

void FilePane::setCloudListing(const QString& folderId,
                                const QString& folderName,
                                const QString& parentId)
{
    if (filterEdit_) filterEdit_->clear();
    currentCloudId_     = folderId;
    currentCloudParent_ = parentId;
    currentCloudName_   = folderName.isEmpty()
                              ? QStringLiteral("My Files")
                              : folderName;
    updatePathLabel();
}

void FilePane::updatePathLabel()
{
    if (type_ == PaneType::Local) {
        pathLabel_->setText(currentLocalPath_);
    } else {
        pathLabel_->setText(QStringLiteral("☁ %1").arg(currentCloudName_));
    }
}

void FilePane::on_itemActivated(const QModelIndex& index)
{
    if (type_ == PaneType::Local) {
        if (upProxy_->isUpEntry(index)) { on_upButton_clicked(); return; }
        const QModelIndex srcIdx = upProxy_->mapToSource(index);
        const QString path = localModel_->filePath(srcIdx);
        if (localModel_->isDir(srcIdx)) setLocalPath(path);
    } else {
        if (cloudFilterProxy_->isUpEntryAt(index)) { on_upButton_clicked(); return; }
        const auto* item = cloudFilterProxy_->itemAt(index);
        if (item && item->isFolder()) emit navigateCloudRequested(item->id);
    }
}

void FilePane::on_upButton_clicked()
{
    if (type_ == PaneType::Local) {
        QDir dir(currentLocalPath_);
        if (dir.cdUp()) {
            setLocalPath(dir.absolutePath());
        }
    } else {
        if (!currentCloudParent_.isEmpty()) {
            emit navigateCloudRequested(currentCloudParent_);
        } else if (!currentCloudId_.isEmpty()) {
            emit navigateCloudRequested({});
        }
    }
}

void FilePane::on_createFolder_clicked()
{
    emit createFolderRequested(currentCloudId_);
}

void FilePane::on_delete_clicked()
{
    const auto selected = view_->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    if (type_ == PaneType::Cloud) {
        for (const auto& idx : selected) {
            const auto* item = cloudFilterProxy_->itemAt(idx);
            if (!item) continue;
            emit deleteRequested(item->id, item->isFolder());
        }
    }
}

void FilePane::on_contextMenu(const QPoint& pos)
{
    const auto idx = view_->indexAt(pos);
    QMenu menu(this);

    if (type_ == PaneType::Cloud) {
        if (idx.isValid()) {
            const auto* item = cloudFilterProxy_->itemAt(idx);
            if (item) {
                const QString itemId      = item->id;
                const bool    itemIsFolder = item->isFolder();

                // Targets: full selection if right-clicked item is in it, else just that item.
                const bool idxInSelection = view_->selectionModel()->isSelected(idx);
                const QList<QModelIndex> targets = idxInSelection
                    ? view_->selectionModel()->selectedIndexes()
                    : QList<QModelIndex>{idx};

                // Snapshot item values now — immune to model reset before user clicks.
                QList<api::FolderItem> downloadTargets;
                for (const auto& si : targets) {
                    if (cloudFilterProxy_->isUpEntryAt(si)) continue;
                    const auto* f = cloudFilterProxy_->itemAt(si);
                    if (!f) continue;
                    if ((f->link.has_value() && !f->link->isEmpty()) || f->isFolder())
                        downloadTargets.append(*f);
                }
                if (!downloadTargets.isEmpty()) {
                    auto* dlAct = menu.addAction("Download");
                    connect(dlAct, &QAction::triggered, this, [this, downloadTargets]() {
                        for (const api::FolderItem& f : downloadTargets) {
                            if (f.link.has_value() && !f.link->isEmpty()) {
                                const bool    isDir = f.isFolder();
                                const QString name  = f.name + (isDir ? ".zip" : "");
                                emit downloadRequested(*f.link,
                                                       currentLocalPath_ + "/" + name,
                                                       f.size.value_or(-1), name);
                            } else if (f.isFolder()) {
                                const QString zipName = f.name + ".zip";
                                emit folderDownloadRequested(f.id, zipName,
                                                             currentLocalPath_ + "/" + zipName);
                            }
                        }
                    });
                }
                auto* delAct = menu.addAction("Delete");
                connect(delAct, &QAction::triggered, this, [this, itemId, itemIsFolder]() {
                    emit deleteRequested(itemId, itemIsFolder);
                });
                menu.addSeparator();
            }
        }
        auto* newFolderAct = menu.addAction("New Folder…");
        connect(newFolderAct, &QAction::triggered, this,
                [this]() { emit createFolderRequested(currentCloudId_); });
    }

    if (type_ == PaneType::Local) {
        QStringList paths;
        const auto selected = view_->selectionModel()->selectedIndexes();
        if (!selected.isEmpty()) {
            for (const auto& si : selected) {
                if (upProxy_->isUpEntry(si)) continue;
                const auto srcIdx = upProxy_->mapToSource(si);
                paths << localModel_->filePath(srcIdx);
            }
        } else if (idx.isValid() && !upProxy_->isUpEntry(idx)) {
            const auto srcIdx = upProxy_->mapToSource(idx);
            paths << localModel_->filePath(srcIdx);
        }
        if (!paths.isEmpty() && !uploadTargetFolderId_.isEmpty()) {
            auto* uploadAct = menu.addAction("Upload to Cloud");
            connect(uploadAct, &QAction::triggered, this, [this, paths]() {
                emit uploadRequested(paths, uploadTargetFolderId_);
            });
        }
    }

    if (!menu.isEmpty()) {
        menu.exec(view_->viewport()->mapToGlobal(pos));
    }
}

// --- Keyboard ---

void FilePane::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const auto idx = view_->currentIndex();
        if (idx.isValid()) {
            on_itemActivated(idx);
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

// --- Drag & Drop ---

void FilePane::dragEnterEvent(QDragEnterEvent* event)
{
    if (type_ == PaneType::Cloud && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else if (type_ == PaneType::Local &&
               event->mimeData()->hasFormat(kCloudMime)) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FilePane::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();
}

void FilePane::dropEvent(QDropEvent* event)
{
    const auto* mime = event->mimeData();

    if (type_ == PaneType::Cloud && mime->hasUrls()) {
        QStringList paths;
        for (const auto& url : mime->urls()) {
            if (url.isLocalFile()) {
                paths << url.toLocalFile();
            }
        }
        if (!paths.isEmpty()) {
            emit uploadRequested(paths, currentCloudId_);
        }
        event->acceptProposedAction();
        return;
    }

    if (type_ == PaneType::Local && mime->hasFormat(kCloudMime)) {
        const auto items = QJsonDocument::fromJson(mime->data(kCloudMime)).array();
        for (const auto& val : items) {
            const auto obj = val.toObject();
            if (!obj.value("isFolder").toBool() && obj.contains("link")) {
                const QString name = obj.value("name").toString();
                const QString dest = currentLocalPath_ + "/" + name;
                const qint64  size = static_cast<qint64>(obj.value("size").toDouble(-1));
                emit downloadRequested(obj.value("link").toString(), dest, size, name);
            }
        }
        event->acceptProposedAction();
        return;
    }

    event->ignore();
}
