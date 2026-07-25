#include "mainwindow.h"
#include "src/windows/mainwindowstyler.h"
#include "ui_mainwindow.h"

#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    MainWindowStyler::apply(*this);
    ui->centralwidget->setStyleSheet(QStringLiteral("background-color: #050505;"));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }

    QMainWindow::keyPressEvent(event);
}
