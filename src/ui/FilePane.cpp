#include "FilePane.hpp"
#include "model/PremiumizeModel.hpp"

#include <QAction>
#include <QDragEnterEvent>
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

    pathLabel_ = new QLabel(this);
    pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    toolbar->addWidget(pathLabel_);
    toolbar->addSeparator();

    auto* upAct = toolbar->addAction("↑ Up");
    connect(upAct, &QAction::triggered, this, &FilePane::on_upButton_clicked);

    if (type_ == PaneType::Cloud) {
        auto* refreshAct = toolbar->addAction("⟳");
        connect(refreshAct, &QAction::triggered, this, &FilePane::refreshRequested);

        auto* newFolderAct = toolbar->addAction("+ Folder");
        connect(newFolderAct, &QAction::triggered, this, &FilePane::on_createFolder_clicked);

        auto* deleteAct = toolbar->addAction("Delete");
        connect(deleteAct, &QAction::triggered, this, &FilePane::on_delete_clicked);
    }

    vl->addWidget(toolbar);

    view_ = new QListView(this);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setDragEnabled(true);
    view_->setAcceptDrops(true);
    view_->setDropIndicatorShown(true);
    view_->setDragDropMode(QAbstractItemView::DragDrop);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view_, &QListView::activated,             this, &FilePane::on_itemActivated);
    connect(view_, &QListView::customContextMenuRequested, this, &FilePane::on_contextMenu);

    vl->addWidget(view_);
}

void FilePane::setupLocalModel()
{
    localModel_ = new QFileSystemModel(this);
    localModel_->setRootPath({});
    localModel_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    view_->setModel(localModel_);
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
    const auto idx = localModel_->setRootPath(path);
    view_->setRootIndex(idx);
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
        const QString path = localModel_->filePath(index);
        if (localModel_->isDir(index)) {
            setLocalPath(path);
        }
    } else {
        const auto& item = cloudModel_->itemAt(index.row());
        if (item.isFolder()) {
            emit navigateCloudRequested(item.id);
        }
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
            const auto& item = cloudModel_->itemAt(idx.row());
            emit deleteRequested(item.id, item.isFolder());
        }
    }
}

void FilePane::on_contextMenu(const QPoint& pos)
{
    const auto idx = view_->indexAt(pos);
    QMenu menu(this);

    if (type_ == PaneType::Cloud) {
        if (idx.isValid()) {
            const auto& item = cloudModel_->itemAt(idx.row());
            if (!item.isFolder()) {
                auto* dlAct = menu.addAction("Download");
                connect(dlAct, &QAction::triggered, this, [this, item]() {
                    if (item.link) {
                        const QString dest = currentLocalPath_ + "/" + item.name;
                        emit downloadRequested(*item.link, dest,
                                               item.size.value_or(-1), item.name);
                    }
                });
            }
            auto* delAct = menu.addAction("Delete");
            connect(delAct, &QAction::triggered, this, [this, item]() {
                emit deleteRequested(item.id, item.isFolder());
            });
            menu.addSeparator();
        }
        auto* newFolderAct = menu.addAction("New Folder…");
        connect(newFolderAct, &QAction::triggered, this,
                [this]() { emit createFolderRequested(currentCloudId_); });
    }

    if (!menu.isEmpty()) {
        menu.exec(view_->viewport()->mapToGlobal(pos));
    }
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
