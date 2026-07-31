#include "mainwindow.h"
#include "src/app/guiapplicationcontroller.h"
#include "src/models/telemetrymodel.h"

#include <QApplication>
#include <QDebug>
#include <QFile>

#include <rclcpp/rclcpp.hpp>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("AURIX MediRun Dashboard"));
    application.setOrganizationName(QStringLiteral("AURIX"));

    qRegisterMetaType<TelemetrySnapshot>("TelemetrySnapshot");
    rclcpp::init(argc, argv);

    const QString qssPath = QStringLiteral(":/styles/aurix_dark.qss");
    const QString logoPath = QStringLiteral(":/images/aurix_logo_01.png");
    const QString splashPath = QStringLiteral(":/assets/aurix_splash.gif");

    qInfo() << "AURIX QSS exists:" << QFile::exists(qssPath);
    qInfo() << "AURIX logo exists:" << QFile::exists(logoPath);
    qInfo() << "AURIX splash exists:" << QFile::exists(splashPath);

    QFile styleFile(qssPath);
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString styleSheet = QString::fromUtf8(styleFile.readAll());
        qInfo() << "AURIX QSS load success. Length:" << styleSheet.size();
        application.setStyleSheet(styleSheet);
    } else {
        qWarning() << "AURIX QSS load failed:" << styleFile.errorString();
    }

    GuiApplicationController controller(application);
    controller.run();

    const int exitCode = QApplication::exec();
    rclcpp::shutdown();
    return exitCode;
}
