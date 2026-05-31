#pragma once
#include "api/ApiTypes.hpp"

#include <QAbstractListModel>
#include <vector>

class PremiumizeModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole      = Qt::DisplayRole,
        IdRole        = Qt::UserRole + 1,
        TypeRole,
        SizeRole,
        CreatedAtRole,
        LinkRole,
        MimeTypeRole,
    };

    explicit PremiumizeModel(QObject* parent = nullptr);

    void populate(const api::FolderListing& listing);
    void clear();

    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    Qt::DropActions supportedDropActions() const override;
    QStringList     mimeTypes() const override;
    QMimeData*      mimeData(const QModelIndexList& indexes) const override;
    bool            canDropMimeData(const QMimeData* data, Qt::DropAction action,
                                    int row, int column,
                                    const QModelIndex& parent) const override;

    const api::FolderItem& itemAt(int row) const;
    // Returns nullptr when viewRow is the up entry or out of bounds.
    const api::FolderItem* itemAtViewRow(int viewRow) const;
    bool isUpEntry(int viewRow) const { return showUpEntry() && viewRow == 0; }

    QString currentFolderId() const { return currentFolderId_; }
    QString parentFolderId()  const { return parentFolderId_; }

private:
    bool showUpEntry() const { return !currentFolderId_.isEmpty() || !parentFolderId_.isEmpty(); }

    std::vector<api::FolderItem> items_;
    QString                      currentFolderId_;
    QString                      parentFolderId_;
};
