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
    // rclcpp::init()이 기본으로 SIGINT/SIGTERM 핸들러를 설치해 ROS
    // 컨텍스트만 종료시키는데, 이게 QApplication 이벤트 루프 종료와 전혀
    // 연결돼 있지 않아서 터미널에서 Ctrl+C/kill로 끄면 rclcpp만 죽고 Qt GUI
    // 프로세스는 그대로 남아있었다(2026-08-14 확인 — SIGTERM/SIGINT로도 안
    // 죽어서 SIGKILL이 필요했음). on_shutdown() 콜백은 신호로 트리거됐든
    // 명시적 rclcpp::shutdown() 호출이든 항상 불리므로, 여기서 Qt 종료까지
    // 이어준다. invokeMethod를 Qt::QueuedConnection으로 쓰는 이유는 이
    // 콜백이 GUI 스레드가 아닌 곳에서 실행될 수 있어서다.
    rclcpp::on_shutdown([]() { QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection); });

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
