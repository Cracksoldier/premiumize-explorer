#pragma once
#include <QWidget>

class QPlainTextEdit;

class LogWindow : public QWidget
{
    Q_OBJECT
public:
    explicit LogWindow(QWidget* parent = nullptr);

public slots:
    void appendEntry(const QString& text);

private slots:
    void on_saveToFile_clicked();
    void on_clear_clicked();

private:
    QPlainTextEdit* logView_;
};
