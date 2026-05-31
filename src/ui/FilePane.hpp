#pragma once
#include <QWidget>
#include <QString>
#include <QStringList>

class QListView;
class QLabel;
class QToolBar;
class QFileSystemModel;
class PremiumizeModel;
class UpEntryProxyModel;

class FilePane : public QWidget
{
    Q_OBJECT
public:
    enum class PaneType { Local, Cloud };

    explicit FilePane(PaneType type, QWidget* parent = nullptr);

    void setLocalPath(const QString& path);
    void setDownloadPath(const QString& path) { currentLocalPath_ = path; }
    void setCloudListing(const QString& folderId,
                         const QString& folderName,
                         const QString& parentId);

    QString currentLocalPath()   const { return currentLocalPath_; }
    QString currentCloudId()     const { return currentCloudId_; }
    QString currentCloudParent() const { return currentCloudParent_; }

    PremiumizeModel* cloudModel() const { return cloudModel_; }

signals:
    void navigateCloudRequested(QString folderId);
    void uploadRequested(QStringList localPaths, QString targetFolderId);
    void downloadRequested(QString remoteUrl, QString localDestPath,
                           qint64 expectedSize, QString itemName);
    void createFolderRequested(QString parentId);
    void deleteRequested(QString itemId, bool isFolder);
    void localPathChanged(QString newPath);
    void refreshRequested();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void on_itemActivated(const QModelIndex& index);
    void on_upButton_clicked();
    void on_createFolder_clicked();
    void on_delete_clicked();
    void on_contextMenu(const QPoint& pos);

private:
    void setupLayout();
    void setupLocalModel();
    void setupCloudModel();
    void updatePathLabel();

    PaneType         type_;
    QListView*       view_;
    QLabel*          pathLabel_;

    QFileSystemModel*  localModel_  = nullptr;
    UpEntryProxyModel* upProxy_     = nullptr;
    PremiumizeModel*   cloudModel_  = nullptr;

    QString currentLocalPath_;
    QString currentCloudId_;
    QString currentCloudParent_;
    QString currentCloudName_;
};
