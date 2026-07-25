#include "mainwindowstyler.h"

#include "../../mainwindow.h"

#include <QMenuBar>
#include <QStatusBar>

void MainWindowStyler::apply(MainWindow &window)
{
    window.setWindowTitle(MainWindow::tr("AURIX Dashboard"));
    window.setStyleSheet(
        "QMainWindow {"
        "background-color: #090909;"
        "color: #f6d37a;"
        "}"
        "QMenuBar, QStatusBar {"
        "background-color: #111111;"
        "color: #f6d37a;"
        "}"
    );

    if (window.menuBar() != nullptr) {
        window.menuBar()->hide();
    }

    if (window.statusBar() != nullptr) {
        window.statusBar()->hide();
    }
}
