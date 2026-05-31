#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;

class ApiKeyDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ApiKeyDialog(QWidget* parent = nullptr);

    QString apiKey() const;

private:
    QLineEdit* keyEdit_;
};
