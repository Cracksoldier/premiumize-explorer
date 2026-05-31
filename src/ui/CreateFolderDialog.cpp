#include "CreateFolderDialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

CreateFolderDialog::CreateFolderDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("New Folder");
    setMinimumWidth(300);

    auto* layout = new QVBoxLayout(this);
    auto* form   = new QFormLayout;
    nameEdit_    = new QLineEdit(this);
    nameEdit_->setPlaceholderText("Folder name");
    form->addRow("Name:", nameEdit_);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(buttons);

    connect(nameEdit_, &QLineEdit::textChanged, this, [buttons](const QString& t) {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!t.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString CreateFolderDialog::folderName() const
{
    return nameEdit_->text().trimmed();
}
