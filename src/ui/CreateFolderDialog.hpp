#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;

class CreateFolderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CreateFolderDialog(QWidget* parent = nullptr);
    QString folderName() const;

private:
    QLineEdit* nameEdit_;
};
