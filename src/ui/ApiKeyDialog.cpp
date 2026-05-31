#include "ApiKeyDialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ApiKeyDialog::ApiKeyDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Premiumize.me — API Key");
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);

    auto* infoLabel = new QLabel(
        "Enter your Premiumize.me API key.<br>"
        "You can find it at <a href=\"https://www.premiumize.me/account\">"
        "premiumize.me/account</a> under <b>API</b>.",
        this);
    infoLabel->setOpenExternalLinks(true);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    auto* form  = new QFormLayout;
    keyEdit_    = new QLineEdit(this);
    keyEdit_->setPlaceholderText("Paste your API key here…");
    keyEdit_->setEchoMode(QLineEdit::Password);
    form->addRow("API Key:", keyEdit_);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(buttons);

    connect(keyEdit_, &QLineEdit::textChanged, this, [buttons](const QString& text) {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!text.trimmed().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString ApiKeyDialog::apiKey() const
{
    return keyEdit_->text().trimmed();
}
