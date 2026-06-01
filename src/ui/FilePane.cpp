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
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

static constexpr const char* kCloudMime = "application/x-premiumize-items";

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
    cloudModel_ = new PremiumizeModel(this);
    view_->setModel(cloudModel_);
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
        if (cloudModel_->isUpEntry(index.row())) { on_upButton_clicked(); return; }
        const auto* item = cloudModel_->itemAtViewRow(index.row());
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
            const auto* item = cloudModel_->itemAtViewRow(idx.row());
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
            const auto* item = cloudModel_->itemAtViewRow(idx.row());
            if (item) {
                const QString itemId      = item->id;
                const bool    itemIsFolder = item->isFolder();
                if (!itemIsFolder) {
                    auto* dlAct = menu.addAction("Download");
                    connect(dlAct, &QAction::triggered, this, [this, itemId]() {
                        for (int r = 0; r < cloudModel_->rowCount(); ++r) {
                            const auto* f = cloudModel_->itemAtViewRow(r);
                            if (f && f->id == itemId && f->link) {
                                emit downloadRequested(*f->link,
                                                       currentLocalPath_ + "/" + f->name,
                                                       f->size.value_or(-1), f->name);
                                return;
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
