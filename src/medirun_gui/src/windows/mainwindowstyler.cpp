#include "mainwindowstyler.h"

#include "../../mainwindow.h"

#include <QMenuBar>
#include <QStatusBar>

void MainWindowStyler::apply(MainWindow &window)
{
    window.setWindowTitle(MainWindow::tr("AURIX Dashboard"));

    if (window.menuBar() != nullptr) {
        window.menuBar()->hide();
    }

    if (window.statusBar() != nullptr) {
        window.statusBar()->hide();
    }
}
