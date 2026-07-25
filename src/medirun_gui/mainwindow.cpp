#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QKeyEvent>
#include <QMenuBar>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("AURIX Dashboard"));
    setStyleSheet(
        "QMainWindow {"
        "background-color: #090909;"
        "color: #f6d37a;"
        "}"
        "QMenuBar, QStatusBar {"
        "background-color: #111111;"
        "color: #f6d37a;"
        "}"
    );
    menuBar()->hide();
    statusBar()->hide();
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
