#include "LogWindow.hpp"

#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextStream>
#include <QVBoxLayout>

LogWindow::LogWindow(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::Tool)
{
    setWindowTitle("API Log");
    resize(700, 400);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(logView_);

    auto* btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(0, 0, 0, 0);

    auto* saveBtn  = new QPushButton("Save to File…", this);
    auto* clearBtn = new QPushButton("Clear", this);

    btnRow->addWidget(saveBtn);
    btnRow->addStretch();
    btnRow->addWidget(clearBtn);
    layout->addLayout(btnRow);

    connect(saveBtn,  &QPushButton::clicked, this, &LogWindow::on_saveToFile_clicked);
    connect(clearBtn, &QPushButton::clicked, this, &LogWindow::on_clear_clicked);
}

void LogWindow::appendEntry(const QString& text)
{
    logView_->appendPlainText(text);
    logView_->verticalScrollBar()->setValue(logView_->verticalScrollBar()->maximum());
}

void LogWindow::on_saveToFile_clicked()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Save API Log", {}, "Text files (*.txt);;All files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << logView_->toPlainText();
}

void LogWindow::on_clear_clicked()
{
    logView_->clear();
}
