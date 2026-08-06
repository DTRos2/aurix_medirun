#include "mainwindow.h"
#include "src/services/authservice.h"
#include "src/services/missioncontroller.h"
#include "src/services/waypointrepository.h"
#include "src/services/telemetryprovider.h"
#include "src/widgets/dashboardcentralwidget.h"
#include "src/windows/mainwindowstyler.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QVBoxLayout>

namespace {
bool isManualControlKey(int key)
{
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_W:
    case Qt::Key_A:
    case Qt::Key_S:
    case Qt::Key_D:
    case Qt::Key_Space:
        return true;
    default:
        return false;
    }
}
} // namespace

MainWindow::MainWindow(const AppSession &session, AuthService &authService, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dashboardWidget(nullptr)
    , m_authService(authService)
    , m_session(session)
    , m_missionController(nullptr)
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
    connect(m_dashboardWidget, &DashboardCentralWidget::logoutRequested, this, &MainWindow::handleLogout);

    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    delete ui;
}

void MainWindow::setTelemetryProvider(TelemetryProvider *provider)
{
    connect(provider,
            &TelemetryProvider::snapshotUpdated,
            m_dashboardWidget,
            &DashboardCentralWidget::applySnapshot,
            Qt::QueuedConnection);
}

void MainWindow::setNavigationService(NavigationService *service)
{
    m_dashboardWidget->setNavigationService(service);
}

void MainWindow::setCameraFeedProvider(CameraFeedProvider *provider)
{
    m_dashboardWidget->setCameraFeedProvider(provider);
}

void MainWindow::setMissionController(MissionController *controller)
{
    m_missionController = controller;
    m_dashboardWidget->setMissionController(controller);
}

void MainWindow::setWaypointRepository(WaypointRepository *repository)
{
    m_dashboardWidget->setWaypointRepository(repository);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!isActiveWindow() || m_dashboardWidget == nullptr) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->isAutoRepeat() && isManualControlKey(keyEvent->key())) {
        return true;
    }

    const bool handled = (event->type() == QEvent::KeyPress)
        ? m_dashboardWidget->handleManualKeyPress(keyEvent->key())
        : m_dashboardWidget->handleManualKeyRelease(keyEvent->key());
    if (handled) {
        event->accept();
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }

    if (!event->isAutoRepeat() && m_dashboardWidget != nullptr
        && m_dashboardWidget->handleManualKeyPress(event->key())) {
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat() && m_dashboardWidget != nullptr
        && m_dashboardWidget->handleManualKeyRelease(event->key())) {
        event->accept();
        return;
    }

    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::handleDashboardExit()
{
    const auto confirm = QMessageBox::question(
        this, QStringLiteral("대시보드 종료"), QStringLiteral("대시보드를 종료하시겠습니까?"));
    if (confirm == QMessageBox::Yes) {
        close();
    }
}

void MainWindow::handleEmergencyStop()
{
    QMessageBox::warning(this, QStringLiteral("비상 정지"), QStringLiteral("비상 정지 요청을 전송합니다."));
}

void MainWindow::handleMissionCancel()
{
    if (m_missionController != nullptr) {
        m_missionController->requestMissionCancel();
    }
}

void MainWindow::handleReturnHome()
{
    if (m_missionController != nullptr) {
        m_missionController->requestReturnHome();
    }
}

void MainWindow::handleLogout()
{
    emit logoutRequested();
    close();
}
