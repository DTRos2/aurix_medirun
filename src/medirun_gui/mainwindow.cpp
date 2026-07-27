#include "mainwindow.h"
#include "src/services/authservice.h"
#include "src/services/telemetryprovider.h"
#include "src/widgets/dashboardcentralwidget.h"
#include "src/windows/mainwindowstyler.h"
#include "ui_mainwindow.h"

#include <QKeyEvent>
#include <QMessageBox>
#include <QVBoxLayout>

MainWindow::MainWindow(const AppSession &session, AuthService &authService, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dashboardWidget(nullptr)
    , m_authService(authService)
    , m_session(session)
{
    ui->setupUi(this);
    MainWindowStyler::apply(*this);
    setMinimumSize(1280, 720);
    resize(1440, 900);

    m_dashboardWidget = new DashboardCentralWidget(m_session, m_authService, ui->centralwidget);
    auto *layout = new QVBoxLayout(ui->centralwidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_dashboardWidget);

    connect(m_dashboardWidget, &DashboardCentralWidget::exitRequested, this, &MainWindow::handleDashboardExit);
    connect(m_dashboardWidget, &DashboardCentralWidget::estopRequested, this, &MainWindow::handleEmergencyStop);
    connect(m_dashboardWidget, &DashboardCentralWidget::missionCancelRequested, this, &MainWindow::handleMissionCancel);
    connect(m_dashboardWidget, &DashboardCentralWidget::returnHomeRequested, this, &MainWindow::handleReturnHome);
    connect(m_dashboardWidget, &DashboardCentralWidget::manualControlRequested, this, &MainWindow::handleManualControl);
    connect(m_dashboardWidget, &DashboardCentralWidget::logoutRequested, this, &MainWindow::handleLogout);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setTelemetryProvider(TelemetryProvider *provider)
{
    connect(provider, &TelemetryProvider::snapshotUpdated, m_dashboardWidget, &DashboardCentralWidget::applySnapshot);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::handleDashboardExit()
{
    close();
}

void MainWindow::handleEmergencyStop()
{
    QMessageBox::warning(this, QStringLiteral("비상 정지"), QStringLiteral("비상 정지 요청을 전송합니다."));
}

void MainWindow::handleMissionCancel()
{
    QMessageBox::information(this, QStringLiteral("미션 취소"), QStringLiteral("미션 취소 요청을 전송합니다."));
}

void MainWindow::handleReturnHome()
{
    QMessageBox::information(this, QStringLiteral("홈 복귀"), QStringLiteral("홈 복귀 요청을 전송합니다."));
}

void MainWindow::handleManualControl()
{
    QMessageBox::information(this, QStringLiteral("원격 수동 제어"), QStringLiteral("관리자 전용 원격 수동 제어 화면은 후속 연동 대상입니다."));
}

void MainWindow::handleLogout()
{
    emit logoutRequested();
    close();
}
