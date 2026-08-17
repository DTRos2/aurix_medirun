#include "remotecontrolpage.h"

#include "../models/waypointtypes.h"
#include "../ros/navigationservice.h"
#include "../services/camerafeedprovider.h"
#include "../services/mapfile.h"
#include "../services/operationsoverviewservice.h"
#include "../services/robotservice.h"
#include "../services/sensortelemetryprovider.h"
#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/mapviewwidget.h"
#include "../widgets/proportionalbar.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QtMath>

namespace {
QLabel *makeLabel(const QString &text, const QString &objectName = {})
{
    auto *label = new QLabel(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    label->setWordWrap(true);
    return label;
}

QFrame *panelFrame(const QString &objectName)
{
    auto *frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

QPushButton *directionButton(const QString &text, bool danger = false)
{
    auto *button = new QPushButton(text);
    button->setObjectName(danger ? QStringLiteral("dangerFilledButton") : QStringLiteral("directionPadButton"));
    button->setFixedSize(38, 38);
    return button;
}

// LiveControlPage::statValueLabel/setLabelColor와 동일한 모양의 "로봇 상태" 행 —
// 센서 상태 패널에서 같은 스타일을 재사용하기 위해 그대로 옮겨왔다.
QLabel *statValueLabel(QBoxLayout *parentLayout, const QString &label)
{
    auto *row = new QHBoxLayout;
    row->addWidget(makeLabel(label, QStringLiteral("metricCaption")));
    row->addStretch();
    auto *valueLabel = makeLabel(QStringLiteral("--"));
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::TextPrimary.name()));
    row->addWidget(valueLabel);
    parentLayout->addLayout(row);
    return valueLabel;
}

void setLabelColor(QLabel *label, const QColor &color)
{
    label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(color.name()));
}

// 로봇이 정지 상태여도 /odom 노이즈가 0 근처에서 부호만 미세하게 흔들려
// "-0.00"/"0.00"이 번갈아 표시되는 문제를 막는다 — EMA로 다듬은 값이라도
// 이 정도 잔류 노이즈는 남는다.
double snapNearZero(double value, double threshold)
{
    return qAbs(value) < threshold ? 0.0 : value;
}
} // namespace

RemoteControlPage::RemoteControlPage(RobotService &robotService, OperationsOverviewService &overviewService, QWidget *parent)
    : QWidget(parent)
    , m_robotService(robotService)
    , m_overviewService(overviewService)
{
    m_teleop = new TeleopController(this);
    connect(m_teleop, &TeleopController::velocityCommandChanged, this, &RemoteControlPage::handleTeleopVelocityChanged);
    connect(m_teleop, &TeleopController::activeChanged, this, &RemoteControlPage::handleTeleopActiveChanged);
    connect(m_teleop, &TeleopController::speedPresetChanged, this, &RemoteControlPage::handleTeleopSpeedPresetChanged);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildManualControlBanner());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    auto *bigSlotWidget = new QWidget(canvas);
    bigSlotWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_bigSlot = new QVBoxLayout(bigSlotWidget);
    m_bigSlot->setContentsMargins(0, 0, 0, 0);
    bodyRow->addWidget(bigSlotWidget, 15);

    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);

    auto *smallSlotWidget = new QWidget;
    smallSlotWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_smallSlot = new QVBoxLayout(smallSlotWidget);
    m_smallSlot->setContentsMargins(0, 0, 0, 0);
    rightCol->addWidget(smallSlotWidget);

    auto *swapButton = buildViewSwapButton();
    swapButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(swapButton);

    auto *loadMapButton = buildLoadMapButton();
    loadMapButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(loadMapButton);

    auto *refreshMarkersButton = buildRefreshMarkersButton();
    refreshMarkersButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(refreshMarkersButton);

    m_cameraPanel = buildCameraPanel();
    m_minimapPanel = buildMinimapPanel();
    m_bigSlot->addWidget(m_cameraPanel);
    m_smallSlot->addWidget(m_minimapPanel);

    auto *directionPad = buildDirectionPad();
    directionPad->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(directionPad);

    auto *speedPresetPanel = buildSpeedPresetPanel();
    speedPresetPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(speedPresetPanel);

    auto *cmdVelPanel = buildCmdVelPanel();
    cmdVelPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(cmdVelPanel);

    auto *sensorStatusPanel = buildSensorStatusPanel();
    sensorStatusPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(sensorStatusPanel);

    rightCol->addStretch(1);

    auto *bottomButtonRow = new QHBoxLayout;
    bottomButtonRow->setSpacing(6);
    auto *dockButton = new QPushButton(QStringLiteral("도킹 재시도"));
    dockButton->setObjectName(QStringLiteral("cyanOutlineButton"));
    dockButton->setMinimumHeight(46);
    auto *autonomyButton = new QPushButton(QStringLiteral("자율 복귀"));
    autonomyButton->setMinimumHeight(46);
    bottomButtonRow->addWidget(dockButton, 1);
    bottomButtonRow->addWidget(autonomyButton, 1);
    rightCol->addLayout(bottomButtonRow);

    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(224);
    rightColWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    layout->addLayout(bodyRow, 1);

    // 기본 큰 화면은 카메라가 아니라 지도 — 진입하자마자 로봇 위치/지도 정렬부터
    // 확인할 수 있도록 한다. swapCameraAndMap()이 이미 큰/작은 슬롯 크기·오버레이
    // 가시성을 전부 처리하므로 그대로 재사용.
    swapCameraAndMap();

    // 페이지 진입 시 지도가 비어 있지 않도록 기본 지도를 자동으로 불러온다
    // (WaypointManagementPage::autoLoadDefaultMap()과 동일한 방식).
    autoLoadDefaultMap();
}

void RemoteControlPage::setCameraFeedProvider(CameraFeedProvider *provider)
{
    if (m_cameraFeedProvider != nullptr) {
        disconnect(m_cameraFeedProvider, nullptr, this, nullptr);
    }

    m_cameraFeedProvider = provider;
    m_lastCameraFrame = QImage();
    applyCameraConnectionState(false);

    if (m_cameraFeedProvider == nullptr) {
        return;
    }

    connect(m_cameraFeedProvider, &CameraFeedProvider::frameReady, this, &RemoteControlPage::applyCameraFrame);
    connect(m_cameraFeedProvider, &CameraFeedProvider::connectionStateChanged, this, &RemoteControlPage::applyCameraConnectionState);
}

void RemoteControlPage::setNavigationService(NavigationService *service)
{
    if (m_navigationService != nullptr) {
        disconnect(m_navigationService, nullptr, this, nullptr);
    }

    m_navigationService = service;
    updateInitialPoseButtonEnabled();

    if (m_navigationService == nullptr) {
        return;
    }

    connect(m_navigationService, &NavigationService::amclPoseUpdated, this, &RemoteControlPage::handleAmclPoseUpdated);
    connect(m_navigationService, &NavigationService::odomVelocityUpdated, this, &RemoteControlPage::handleOdomVelocityUpdated);
    // 웨이포인트 관리 페이지 등 다른 페이지에서 시작한 주행이라도 같은
    // NavigationService를 공유하므로 여기서도 감지된다.
    connect(m_navigationService, &NavigationService::navSessionStarted, this, &RemoteControlPage::handleNavSessionStarted);
    connect(m_navigationService, &NavigationService::navSessionFinished, this, &RemoteControlPage::handleNavSessionFinished);
    // navSessionStarted를 놓친 뒤늦은 연결(이 페이지가 열리기 전에 이미
    // 주행이 시작된 경우)이라도, 즉시 현재 상태로 동기화해서 배너/방향
    // 패드를 맞춰준다.
    if (m_navigationService->isNavigating()) {
        handleNavSessionStarted({}, QString());
    }
    // amcl_pose는 TRANSIENT_LOCAL이라 앱 시작 직후(로그인 전) 이미 한 번
    // 와버렸을 수 있어, 이 페이지가 연결하기 전 시그널을 놓쳤을 수 있다.
    if (double x, y, yaw; m_navigationService->lastKnownAmclPose(x, y, yaw)) {
        handleAmclPoseUpdated(x, y, yaw);
    }
}

void RemoteControlPage::setSensorTelemetryProvider(SensorTelemetryProvider *provider)
{
    if (m_sensorTelemetryProvider == provider) {
        return;
    }
    m_sensorTelemetryProvider = provider;
    if (m_sensorTelemetryProvider != nullptr) {
        connect(m_sensorTelemetryProvider, &SensorTelemetryProvider::frontDistanceChanged, this, &RemoteControlPage::handleFrontDistanceChanged);
        connect(m_sensorTelemetryProvider, &SensorTelemetryProvider::rearDistanceChanged, this, &RemoteControlPage::handleRearDistanceChanged);
        connect(m_sensorTelemetryProvider, &SensorTelemetryProvider::payloadLoadedChanged, this, &RemoteControlPage::handlePayloadLoadedChanged);
    }
}

void RemoteControlPage::setSession(const AppSession &session)
{
    m_session = session;
}

bool RemoteControlPage::handleManualKeyPress(int key)
{
    if (m_navigationInProgress) {
        return false;
    }
    return m_teleop->consumeKey(key);
}

bool RemoteControlPage::handleManualKeyRelease(int key)
{
    if (m_navigationInProgress) {
        return false;
    }
    return m_teleop->releaseKey(key);
}

void RemoteControlPage::hideEvent(QHideEvent *event)
{
    // 다른 페이지로 전환될 때 눌려있던 키/버튼이 있어도 즉시 정지 —
    // 화면을 보고 있지 않은 상태에서 로봇이 계속 움직이면 안 된다.
    m_teleop->stop();
    QWidget::hideEvent(event);
}

void RemoteControlPage::showEvent(QShowEvent *event)
{
    // 웨이포인트 관리 페이지에서 콜존/홈을 추가·수정하고 돌아왔을 때 바로
    // 반영되도록, 페이지가 다시 보일 때마다 DB 마커를 새로 읽어온다.
    // 읽기 전용 조회라 teleop처럼 안전 문제(hideEvent와 비대칭)는 없다.
    refreshDbMarkers();
    QWidget::showEvent(event);
}

void RemoteControlPage::handleTeleopVelocityChanged(double linearX, double angularZ)
{
    if (m_navigationService != nullptr) {
        m_navigationService->driveManual(linearX, angularZ);
    }

    if (m_cmdVelLinearValueLabel != nullptr) {
        m_cmdVelLinearValueLabel->setText(QStringLiteral("%1 m/s").arg(linearX, 0, 'f', 2));
    }
    if (m_cmdVelAngularValueLabel != nullptr) {
        m_cmdVelAngularValueLabel->setText(QStringLiteral("%1 rad/s").arg(angularZ, 0, 'f', 2));
    }
}

void RemoteControlPage::handleOdomVelocityUpdated(double linearMps, double angularRadps)
{
    // teleop이 조작 중이면 그쪽이 명령한 값을 즉시 그대로 보여주는 기존
    // 경로가 더 정확하고 실기 검증도 끝나 있으므로, 실측 /odom 값으로
    // 덮어쓰지 않는다 — 여기서는 웨이포인트 자동주행 중이거나 완전히
    // 대기 중일 때만 실제 속도를 반영한다.
    if (m_teleopActive) {
        return;
    }
    // LiveControlPage와 동일한 alpha=0.25 지수이동평균 — 특히 angular.z가
    // raw로 보면 부호가 계속 뒤집힐 정도로 흔들려서 그대로 표시하면 안 된다.
    m_lastOdomLinearVel = m_lastOdomLinearVel * 0.75 + linearMps * 0.25;
    m_lastOdomAngularVel = m_lastOdomAngularVel * 0.75 + angularRadps * 0.25;
    if (m_cmdVelLinearValueLabel != nullptr) {
        m_cmdVelLinearValueLabel->setText(
            QStringLiteral("%1 m/s").arg(snapNearZero(m_lastOdomLinearVel, 0.01), 0, 'f', 2));
    }
    if (m_cmdVelAngularValueLabel != nullptr) {
        m_cmdVelAngularValueLabel->setText(
            QStringLiteral("%1 rad/s").arg(snapNearZero(m_lastOdomAngularVel, 0.01), 0, 'f', 2));
    }
    if (m_cmdVelTitleLabel != nullptr) {
        m_cmdVelTitleLabel->setText(m_navigationInProgress ? QStringLiteral("자동주행 중 · /odom")
                                                             : QStringLiteral("대기 · /cmd_vel"));
    }
}

void RemoteControlPage::handleTeleopActiveChanged(bool active)
{
    m_teleopActive = active;
    refreshRobotMarkerState();
    if (m_manualBannerTitleLabel != nullptr) {
        m_manualBannerTitleLabel->setText(active ? QStringLiteral("🎮  수동 제어 활성") : QStringLiteral("수동 제어 대기"));
        m_manualBannerTitleLabel->setStyleSheet(
            QStringLiteral("color: %1; font-size: 12px; font-weight: 600;")
                .arg(active ? DashboardPalette::Red.name() : DashboardPalette::TextTertiary.name()));
    }
    if (m_cmdVelTitleLabel != nullptr) {
        m_cmdVelTitleLabel->setText(active ? QStringLiteral("발행 중 · /cmd_vel") : QStringLiteral("대기 · /cmd_vel"));
    }
    if (m_manualBannerCaptionLabel != nullptr) {
        // 여러 PC 동시 조작을 막는 제어권 락은 아직 없다(추후 별도 기능) —
        // 지금은 실제로 이 PC에서 조작 중일 때만, 하드코딩이 아니라 로그인한
        // 계정을 그대로 보여준다.
        m_manualBannerCaptionLabel->setText(
            active ? QStringLiteral("방향 버튼/WASD로 조작 · 제어권 qt / %1").arg(m_session.username)
                   : QStringLiteral("방향 버튼/WASD로 조작"));
    }
}

void RemoteControlPage::handleTeleopSpeedPresetChanged(TeleopController::SpeedPreset preset)
{
    Q_UNUSED(preset);
    updateSpeedPresetButtons();
}

void RemoteControlPage::handleNavSessionStarted(const QVector<NavPose> &goals, const QString &label)
{
    Q_UNUSED(goals);
    Q_UNUSED(label);
    m_navigationInProgress = true;
    refreshRobotMarkerState();
    // 자동주행이 시작됐는데 방향 버튼/키가 눌려있던 상태로 남아있으면
    // teleop과 Nav2가 동시에 /cmd_vel을 발행해 충돌한다 — 즉시 정지.
    m_teleop->stop();
    if (m_directionPad != nullptr) {
        m_directionPad->setEnabled(false);
    }
    if (m_manualBannerTitleLabel != nullptr) {
        m_manualBannerTitleLabel->setText(QStringLiteral("🧭  웨이포인트 이동 중"));
        m_manualBannerTitleLabel->setStyleSheet(
            QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Amber.name()));
    }
    if (m_manualBannerCaptionLabel != nullptr) {
        m_manualBannerCaptionLabel->setText(QStringLiteral("이동이 끝날 때까지 수동 제어를 할 수 없습니다"));
    }
}

void RemoteControlPage::handleNavSessionFinished(bool success, bool cancelled)
{
    Q_UNUSED(success);
    Q_UNUSED(cancelled);
    m_navigationInProgress = false;
    if (m_directionPad != nullptr) {
        m_directionPad->setEnabled(true);
    }
    // 배너 문구/색을 "수동 제어 대기" 상태로 되돌린다 — teleop은 이동 중
    // stop()된 채로 남아있으므로 active=false가 맞다.
    handleTeleopActiveChanged(false);
}

void RemoteControlPage::handleFrontDistanceChanged(int centimeters)
{
    if (m_frontObstacleValueLabel == nullptr) {
        return;
    }
    m_frontObstacleValueLabel->setText(QStringLiteral("%1cm").arg(centimeters));
    setLabelColor(m_frontObstacleValueLabel, centimeters < 20 ? DashboardPalette::Red : DashboardPalette::TextPrimary);
}

void RemoteControlPage::handleRearDistanceChanged(int centimeters)
{
    if (m_rearObstacleValueLabel == nullptr) {
        return;
    }
    m_rearObstacleValueLabel->setText(QStringLiteral("%1cm").arg(centimeters));
    setLabelColor(m_rearObstacleValueLabel, centimeters < 20 ? DashboardPalette::Red : DashboardPalette::TextPrimary);
}

void RemoteControlPage::handlePayloadLoadedChanged(bool loaded)
{
    if (m_payloadValueLabel == nullptr) {
        return;
    }
    m_payloadValueLabel->setText(loaded ? QStringLiteral("적재됨") : QStringLiteral("미적재"));
    setLabelColor(m_payloadValueLabel, loaded ? DashboardPalette::Green : DashboardPalette::TextMuted);
}

void RemoteControlPage::updateSpeedPresetButtons()
{
    if (m_speedFineButton == nullptr || m_speedNormalButton == nullptr || m_speedFastButton == nullptr) {
        return;
    }

    const TeleopController::SpeedPreset preset = m_teleop->speedPreset();
    auto applyStyle = [](QPushButton *button, bool selected) {
        button->setObjectName(selected ? QStringLiteral("cyanChipButton") : QStringLiteral("ghostChipButton"));
        button->style()->unpolish(button);
        button->style()->polish(button);
    };
    applyStyle(m_speedFineButton, preset == TeleopController::SpeedPreset::Fine);
    applyStyle(m_speedNormalButton, preset == TeleopController::SpeedPreset::Normal);
    applyStyle(m_speedFastButton, preset == TeleopController::SpeedPreset::Fast);

    if (m_linearSpeedValueLabel != nullptr) {
        m_linearSpeedValueLabel->setText(QStringLiteral("%1").arg(TeleopController::linearSpeedForPreset(preset), 0, 'f', 2));
    }
    if (m_angularSpeedValueLabel != nullptr) {
        m_angularSpeedValueLabel->setText(QStringLiteral("%1").arg(TeleopController::angularSpeedForPreset(preset), 0, 'f', 2));
    }
}

void RemoteControlPage::handleAmclPoseUpdated(double x, double y, double yawRad)
{
    if (!m_hasAmclPose) {
        // 초기 위치를 처음 받은 순간 — 안내 배너를 "일치 확인"으로 바꾸고,
        // 이제부터 로봇 마커를 그릴 수 있는 상태가 된다.
        m_hasAmclPose = true;
        updateInitialPoseBanner();
    }

    if (!m_mapIsMain || !m_currentMapMeta.valid) {
        return;
    }

    m_mapView->setRobotVisible(true);
    m_mapView->setRobotPose(worldToNormalized(x, y), qRadiansToDegrees(yawRad));
    refreshRobotMarkerState();
}

void RemoteControlPage::refreshRobotMarkerState()
{
    if (m_mapView == nullptr) {
        return;
    }
    const bool driving = m_teleopActive || m_navigationInProgress;
    m_mapView->setRobotState(driving ? RobotMarkerState::Moving : RobotMarkerState::Ready);
}

void RemoteControlPage::applyCameraFrame(const QImage &frame)
{
    if (m_cameraFrameLabel == nullptr) {
        return;
    }

    m_lastCameraFrame = frame;
    m_cameraFrameLabel->setText(QString());
    m_cameraFrameLabel->setPixmap(
        QPixmap::fromImage(frame).scaled(m_cameraFrameLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void RemoteControlPage::applyCameraConnectionState(bool connected)
{
    if (m_cameraFrameLabel == nullptr || connected) {
        return;
    }

    m_lastCameraFrame = QImage();
    m_cameraFrameLabel->setPixmap(QPixmap());
    m_cameraFrameLabel->setText(QStringLiteral("🎥  카메라 연결 대기"));
}

void RemoteControlPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (!m_lastCameraFrame.isNull()) {
        applyCameraFrame(m_lastCameraFrame);
    }
}

QWidget *RemoteControlPage::buildManualControlBanner()
{
    auto *panel = panelFrame(QStringLiteral("anomalyCard"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(8);

    m_manualBannerTitleLabel = makeLabel(QStringLiteral("수동 제어 대기"));
    m_manualBannerTitleLabel->setWordWrap(false);
    m_manualBannerTitleLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::TextTertiary.name()));
    layout->addWidget(m_manualBannerTitleLabel);
    m_manualBannerCaptionLabel = makeLabel(QStringLiteral("방향 버튼/WASD로 조작"), QStringLiteral("metricCaption"));
    m_manualBannerCaptionLabel->setWordWrap(false);
    layout->addWidget(m_manualBannerCaptionLabel);
    layout->addStretch();
    return panel;
}

QWidget *RemoteControlPage::buildCameraPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    panel->setMinimumHeight(230);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(11, 9, 11, 9);

    m_cameraTopRow = new QWidget(panel);
    auto *topRow = new QHBoxLayout(m_cameraTopRow);
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addWidget(makeBadgeLabel(QStringLiteral("ArUco 미검출"), QStringLiteral("amber"), panel));
    topRow->addStretch();
    topRow->addWidget(makeLabel(QStringLiteral("지연 120ms"), QStringLiteral("metricCaption")));
    layout->addWidget(m_cameraTopRow);

    m_cameraFrameLabel = makeLabel(QStringLiteral("🎥  카메라 연결 대기"));
    m_cameraFrameLabel->setAlignment(Qt::AlignCenter);
    m_cameraFrameLabel->setStyleSheet(QStringLiteral("color: %1;").arg(DashboardPalette::TextTertiary.name()));
    m_cameraFrameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_cameraFrameLabel->setMinimumSize(1, 1);
    layout->addWidget(m_cameraFrameLabel, 1);

    return panel;
}

QWidget *RemoteControlPage::buildSensorStatusPanel()
{
    // LiveControlPage::buildRobotStatusPanel()의 전방/후방 장애물·AED 적재
    // 항목을 그대로 재사용한다 — 좌/우 초음파 센서는 실제 하드웨어가 없어
    // 표시하지 않는다.
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(11, 9, 11, 9);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("센서 상태"), QStringLiteral("metricCaption")));

    m_frontObstacleValueLabel = statValueLabel(layout, QStringLiteral("전방 장애물"));
    m_rearObstacleValueLabel = statValueLabel(layout, QStringLiteral("후방 장애물"));
    m_payloadValueLabel = statValueLabel(layout, QStringLiteral("AED 적재"));
    return panel;
}

QWidget *RemoteControlPage::buildMinimapPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    // MapViewWidget already paints its own dashed extent border, so the
    // panel's own QSS border is dropped here to avoid a doubled box outline.
    panel->setStyleSheet(QStringLiteral("background-color: %1; border: none; border-radius: 8px;").arg(DashboardPalette::PanelBg.name()));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // 미니맵(작은) 상태에서는 실시간 위치/초기 위치 설정이 의미 없고 클릭
    // 정밀도도 떨어지므로, 큰 화면(m_mapIsMain)일 때만 보이게 한다 —
    // swapCameraAndMap()에서 visibility를 토글.
    m_mapPoseHintBanner = buildPoseHintBanner();
    layout->addWidget(m_mapPoseHintBanner);

    m_mapView = new MapViewWidget(panel);
    // Fixed height only; width follows the panel so the dashed extent rect
    // (drawn relative to the widget's own size) hugs both left and right
    // edges symmetrically instead of leaving dead space on the right.
    m_mapView->setFixedHeight(100);
    m_mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_mapView->setMode(MapViewMode::Reference);
    m_mapView->setCaption(QStringLiteral("미니맵 · 방향 참조용"));
    // 초기 위치를 아직 못 받았으면(m_hasAmclPose==false) 로봇 마커를 그리지
    // 않는다 — 위치를 모르는 채로 마커부터 보이면 오해를 준다. 대신
    // buildPoseHintBanner()의 안내 배너로 "초기 위치를 설정하세요"를 보여준다.
    m_mapView->setRobotVisible(false);
    connect(m_mapView, &MapViewWidget::posePickRequested, this, &RemoteControlPage::handlePosePickRequested);
    layout->addWidget(m_mapView, 1);

    m_setInitialPoseButton = new QPushButton(QStringLiteral("📍  초기 위치 설정"));
    m_setInitialPoseButton->setObjectName(QStringLiteral("ghostChipButton"));
    m_setInitialPoseButton->setCheckable(true);
    m_setInitialPoseButton->setEnabled(false);
    m_setInitialPoseButton->setToolTip(
        QStringLiteral("RViz의 2D Pose Estimate와 동일 · 켠 뒤 지도를 클릭+드래그하면 그 위치·방향으로 AMCL 초기 위치를 설정합니다"));
    connect(m_setInitialPoseButton, &QPushButton::toggled, this, &RemoteControlPage::handleSetInitialPoseToggled);
    layout->addWidget(m_setInitialPoseButton);

    return panel;
}

QWidget *RemoteControlPage::buildPoseHintBanner()
{
    auto *panel = panelFrame(QStringLiteral("warningBanner"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(9, 6, 9, 6);
    layout->setSpacing(6);

    m_poseHintIconLabel = makeLabel(QStringLiteral("⚠"));
    layout->addWidget(m_poseHintIconLabel);

    m_poseHintTextLabel = makeLabel(QString());
    layout->addWidget(m_poseHintTextLabel, 1);

    m_mapPoseHintBanner = panel;
    updateInitialPoseBanner();

    return panel;
}

void RemoteControlPage::updateInitialPoseBanner()
{
    if (m_mapPoseHintBanner == nullptr || m_poseHintIconLabel == nullptr || m_poseHintTextLabel == nullptr) {
        return;
    }

    if (!m_hasAmclPose) {
        // 초기 위치를 아예 모르는 상태 — QFrame#warningBanner(주황)보다 급한
        // 빨강으로 눈에 띄게 만든다(buildManualControlBanner의 anomalyCard와
        // 동일 톤).
        m_mapPoseHintBanner->setStyleSheet(
            QStringLiteral("background-color: #1a0f18; border: 1px solid #7c2438; border-radius: 8px;"));
        m_poseHintIconLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Red.name()));
        m_poseHintTextLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Red.name()));
        m_poseHintTextLabel->setText(
            QStringLiteral("초기 위치가 설정되지 않았습니다 · 아래 '📍 초기 위치 설정' 버튼을 누른 뒤 지도를 클릭+드래그하세요"));
        return;
    }

    // 초기 위치를 받은 뒤에는 원래의 온화한 확인 안내로 되돌아간다 —
    // 인라인 스타일을 비워서 QFrame#warningBanner(주황) qss가 다시 적용되게 한다.
    m_mapPoseHintBanner->setStyleSheet(QString());
    m_poseHintIconLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Amber.name()));
    m_poseHintTextLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Amber.name()));
    m_poseHintTextLabel->setText(QStringLiteral("로봇의 실제 위치와 지도 위 마커가 일치하는지 확인하세요 · 다르면 초기 위치 설정"));
}

QWidget *RemoteControlPage::buildViewSwapButton()
{
    auto *button = new QPushButton(QStringLiteral("⇄  지도/카메라 전환"));
    button->setObjectName(QStringLiteral("ghostChipButton"));
    connect(button, &QPushButton::clicked, this, &RemoteControlPage::swapCameraAndMap);
    return button;
}

QWidget *RemoteControlPage::buildLoadMapButton()
{
    auto *button = new QPushButton(QStringLiteral("🗂  지도 불러오기"));
    button->setObjectName(QStringLiteral("ghostChipButton"));
    connect(button, &QPushButton::clicked, this, &RemoteControlPage::handleLoadMap);
    return button;
}

QWidget *RemoteControlPage::buildRefreshMarkersButton()
{
    // 웨이포인트 관리 페이지에서 콜존/홈을 바꾼 뒤 페이지 재진입 없이도
    // 바로 반영하고 싶을 때 수동으로 다시 불러오는 용도(showEvent 자동
    // 갱신과 별개).
    auto *button = new QPushButton(QStringLiteral("🔄  마커 새로고침"));
    button->setObjectName(QStringLiteral("ghostChipButton"));
    connect(button, &QPushButton::clicked, this, &RemoteControlPage::refreshDbMarkers);
    return button;
}

void RemoteControlPage::handleLoadMap()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("지도 불러오기 (nav2 map_server YAML)"), QString(),
        QStringLiteral("맵 YAML (*.yaml *.yml)"));
    if (path.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!loadMapFromFile(path, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("지도 불러오기 실패"), errorMessage);
        return;
    }

    if (!errorMessage.isEmpty()) {
        // 회전(origin theta) 경고 등 — 실패는 아니므로 로드는 이미 적용된 상태.
        QMessageBox::information(this, QStringLiteral("지도 불러오기"), errorMessage);
    }
}

bool RemoteControlPage::loadMapFromFile(const QString &path, QString &errorMessage)
{
    MapMeta meta;
    if (!loadMapYaml(path, meta, errorMessage)) {
        return false;
    }

    m_currentMapMeta = meta;
    m_mapView->setBackgroundImage(meta.image);
    updateInitialPoseButtonEnabled();
    refreshDbMarkers();
    return true;
}

void RemoteControlPage::refreshDbMarkers()
{
    if (!m_currentMapMeta.valid) {
        return;
    }

    // 홈 — waypoint_set.name="home"(kind=Home). 로봇이 정확히 1대일 때만
    // 조회한다(WaypointManagementPage::mergeHomeWaypointInto와 동일 규칙).
    bool hasHome = false;
    QPointF homeNormalized;
    const QVector<RobotRecord> robots = m_robotService.listRobots(false);
    if (robots.size() == 1) {
        QString frameId;
        WaypointMap homeWaypoints;
        TrajectoryMap trajectories;
        QString homeErrorMessage;
        if (m_overviewService.loadWaypointLibraryByName(
                robots.first().id, QStringLiteral("home"), frameId, homeWaypoints, trajectories, homeErrorMessage)) {
            for (auto it = homeWaypoints.cbegin(); it != homeWaypoints.cend(); ++it) {
                if (it.value().kind == WaypointMarkerKind::Home) {
                    homeNormalized = worldToNormalized(it.value().x, it.value().y);
                    hasHome = true;
                    break;
                }
            }
        }
    }
    m_mapView->setHome(homeNormalized, hasHome);

    // 콜존 — call_button 테이블(현재 활성 항목). WaypointManagementPage의
    // 콜존 글리프를 재사용하는 MapViewWidget::setCallZones()로 홈과 구분되게 표시.
    QVector<CallButtonRow> callButtons;
    QString callButtonErrorMessage;
    QVector<QPointF> callZonePoints;
    QStringList callZoneLabels;
    if (m_overviewService.loadActiveCallButtons(callButtons, callButtonErrorMessage)) {
        callZonePoints.reserve(callButtons.size());
        callZoneLabels.reserve(callButtons.size());
        for (const CallButtonRow &row : std::as_const(callButtons)) {
            callZonePoints.append(worldToNormalized(row.x, row.y));
            callZoneLabels.append(row.zoneName.isEmpty() ? row.code : row.zoneName);
        }
    }
    m_mapView->setCallZones(callZonePoints, callZoneLabels);
}

QString RemoteControlPage::workspaceRootPath() const
{
    // WaypointManagementPage::workspaceRootPath()와 동일한 방식 — 실행 경로
    // 깊이는 --symlink-install 여부에 따라 달라지므로, maps/ 디렉터리가
    // 나올 때까지 상위로 탐색한다.
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("maps")))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

void RemoteControlPage::autoLoadDefaultMap()
{
    const QString workspaceRoot = workspaceRootPath();
    if (workspaceRoot.isEmpty()) {
        return;
    }

    const QString defaultMapPath = QDir(workspaceRoot).filePath(QStringLiteral("maps/last_class_map_modi.yaml"));
    if (!QFileInfo::exists(defaultMapPath)) {
        return;
    }

    QString errorMessage;
    loadMapFromFile(defaultMapPath, errorMessage);
}

void RemoteControlPage::handleSetInitialPoseToggled(bool checked)
{
    m_settingInitialPose = checked;
    m_mapView->setPoseSettingEnabled(checked);
}

void RemoteControlPage::handlePosePickRequested(const QPointF &normalizedPos, double yawRadians)
{
    if (!m_settingInitialPose || m_navigationService == nullptr || !m_currentMapMeta.valid) {
        return;
    }

    // 한 번 찍으면 바로 꺼짐 — 웨이포인트 페이지의 "초기 위치 설정"과 동일한 UX.
    m_setInitialPoseButton->setChecked(false);

    const QPointF world = normalizedToWorld(normalizedPos);
    m_navigationService->publishInitialPose(world.x(), world.y(), yawRadians, QStringLiteral("map"));
}

QPointF RemoteControlPage::worldToNormalized(double x, double y) const
{
    const QPointF pixel = worldToPixel(QPointF(x, y), m_currentMapMeta);
    return QPointF(pixel.x() / m_currentMapMeta.width, pixel.y() / m_currentMapMeta.height);
}

QPointF RemoteControlPage::normalizedToWorld(const QPointF &normalized) const
{
    const QPointF pixel(normalized.x() * m_currentMapMeta.width, normalized.y() * m_currentMapMeta.height);
    return pixelToWorld(pixel, m_currentMapMeta);
}

void RemoteControlPage::updateInitialPoseButtonEnabled()
{
    if (m_setInitialPoseButton == nullptr) {
        return;
    }
    m_setInitialPoseButton->setEnabled(m_navigationService != nullptr && m_currentMapMeta.valid);
}

void RemoteControlPage::swapCameraAndMap()
{
    m_mapIsMain = !m_mapIsMain;

    m_bigSlot->removeWidget(m_cameraPanel);
    m_bigSlot->removeWidget(m_minimapPanel);
    m_smallSlot->removeWidget(m_cameraPanel);
    m_smallSlot->removeWidget(m_minimapPanel);

    QWidget *bigWidget = m_mapIsMain ? m_minimapPanel : m_cameraPanel;
    QWidget *smallWidget = m_mapIsMain ? m_cameraPanel : m_minimapPanel;
    m_bigSlot->addWidget(bigWidget);
    m_smallSlot->addWidget(smallWidget);

    if (m_mapIsMain) {
        m_mapView->setMinimumSize(0, 0);
        m_mapView->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_minimapPanel->setMinimumHeight(230);
        m_minimapPanel->setMaximumHeight(QWIDGETSIZE_MAX);
        m_minimapPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_cameraPanel->setMinimumHeight(0);
        // 배지 오버레이를 감춰서 축소된 높이 대부분을 실제 영상에 내준다
        // (100px로는 오버레이만 겨우 보이고 영상은 안 보이던 문제 수정).
        m_cameraTopRow->setVisible(false);
        m_cameraPanel->setFixedHeight(150);
        m_cameraPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

        // 지도가 큰 화면이 됐을 때만 실시간 위치/초기 위치 설정 UI를 노출한다.
        m_mapPoseHintBanner->setVisible(true);
        m_setInitialPoseButton->setVisible(true);
    } else {
        m_mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_mapView->setMinimumWidth(0);
        m_mapView->setMaximumWidth(QWIDGETSIZE_MAX);
        m_mapView->setFixedHeight(100);
        m_minimapPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        m_minimapPanel->setMinimumHeight(0);
        m_minimapPanel->setMaximumHeight(QWIDGETSIZE_MAX);
        m_cameraTopRow->setVisible(true);
        m_cameraPanel->setMaximumHeight(QWIDGETSIZE_MAX);
        m_cameraPanel->setMinimumHeight(230);
        m_cameraPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        // 미니맵으로 축소되면 초기 위치 설정 제스처도 같이 꺼서, 숨겨진 상태에서
        // 지도 클릭 좌표가 엉뚱하게 해석되는 일이 없도록 한다.
        m_mapPoseHintBanner->setVisible(false);
        m_setInitialPoseButton->setChecked(false);
        m_setInitialPoseButton->setVisible(false);
    }
}

QWidget *RemoteControlPage::buildDirectionPad()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QGridLayout(panel);
    layout->setContentsMargins(9, 10, 9, 10);
    layout->setSpacing(5);

    auto *forwardButton = directionButton(QStringLiteral("▲"));
    auto *leftButton = directionButton(QStringLiteral("↺"));
    auto *stopButton = directionButton(QStringLiteral("정지"), true);
    // dangerFilledButton은 전역 QPushButton의 padding: 0 12px를 물려받는데,
    // 38x38 고정 크기 방향패드 버튼 안에 "정지" 두 글자가 들어가면 잘린다.
    // 방향패드 전용 스타일(directionPadStopButton)로 여백/폰트를 다시 잡는다.
    stopButton->setObjectName(QStringLiteral("directionPadStopButton"));
    auto *rightButton = directionButton(QStringLiteral("↻"));
    auto *backwardButton = directionButton(QStringLiteral("▼"));

    for (QPushButton *button : {forwardButton, backwardButton, leftButton, rightButton, stopButton}) {
        button->setFocusPolicy(Qt::NoFocus);
    }

    connect(forwardButton, &QPushButton::pressed, this, [this]() { m_teleop->pressDirection(TeleopController::Direction::Forward); });
    connect(forwardButton, &QPushButton::released, this, [this]() { m_teleop->releaseDirection(TeleopController::Direction::Forward); });
    connect(backwardButton, &QPushButton::pressed, this, [this]() { m_teleop->pressDirection(TeleopController::Direction::Backward); });
    connect(backwardButton, &QPushButton::released, this, [this]() { m_teleop->releaseDirection(TeleopController::Direction::Backward); });
    connect(leftButton, &QPushButton::pressed, this, [this]() { m_teleop->pressDirection(TeleopController::Direction::Left); });
    connect(leftButton, &QPushButton::released, this, [this]() { m_teleop->releaseDirection(TeleopController::Direction::Left); });
    connect(rightButton, &QPushButton::pressed, this, [this]() { m_teleop->pressDirection(TeleopController::Direction::Right); });
    connect(rightButton, &QPushButton::released, this, [this]() { m_teleop->releaseDirection(TeleopController::Direction::Right); });
    connect(stopButton, &QPushButton::clicked, this, [this]() { m_teleop->stop(); });

    layout->addWidget(forwardButton, 0, 1);
    layout->addWidget(leftButton, 1, 0);
    layout->addWidget(stopButton, 1, 1);
    layout->addWidget(rightButton, 1, 2);
    layout->addWidget(backwardButton, 2, 1);
    m_directionPad = panel;
    return panel;
}

QWidget *RemoteControlPage::buildSpeedPresetPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(11, 9, 11, 9);
    layout->setSpacing(7);

    auto *headerRow = new QHBoxLayout;
    headerRow->addWidget(makeLabel(QStringLiteral("속도 프리셋"), QStringLiteral("metricCaption")));
    headerRow->addStretch();
    auto *sharedCaption = makeLabel(QStringLiteral("세 방식 값 공유"));
    sharedCaption->setStyleSheet(QStringLiteral("color: %1; font-size: 9px;").arg(DashboardPalette::TextMuted.name()));
    headerRow->addWidget(sharedCaption);
    layout->addLayout(headerRow);

    auto *presetRow = new QHBoxLayout;
    presetRow->setSpacing(5);
    m_speedFineButton = new QPushButton(QStringLiteral("미세"));
    m_speedNormalButton = new QPushButton(QStringLiteral("보통"));
    m_speedFastButton = new QPushButton(QStringLiteral("빠름"));
    presetRow->addWidget(m_speedFineButton, 1);
    presetRow->addWidget(m_speedNormalButton, 1);
    presetRow->addWidget(m_speedFastButton, 1);
    layout->addLayout(presetRow);

    connect(m_speedFineButton, &QPushButton::clicked, this, [this]() { m_teleop->setSpeedPreset(TeleopController::SpeedPreset::Fine); });
    connect(m_speedNormalButton, &QPushButton::clicked, this, [this]() { m_teleop->setSpeedPreset(TeleopController::SpeedPreset::Normal); });
    connect(m_speedFastButton, &QPushButton::clicked, this, [this]() { m_teleop->setSpeedPreset(TeleopController::SpeedPreset::Fast); });

    layout->addWidget(buildSpeedAxisRow(QStringLiteral("▲ 직진"), QStringLiteral("m/s"), 55, &m_linearSpeedValueLabel));
    layout->addWidget(buildSpeedAxisRow(QStringLiteral("↻ 회전"), QStringLiteral("rad/s"), 28, &m_angularSpeedValueLabel));

    updateSpeedPresetButtons();

    return panel;
}

QWidget *RemoteControlPage::buildSpeedAxisRow(const QString &label, const QString &unit, int percent, QLabel **valueLabelOut)
{
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *row = new QHBoxLayout;
    auto *labelWidget = makeLabel(label);
    labelWidget->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::TextSecondary.name()));
    row->addWidget(labelWidget);
    row->addStretch();

    auto *valueBox = makeLabel(QString());
    valueBox->setAlignment(Qt::AlignCenter);
    valueBox->setFixedSize(50, 22);
    valueBox->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background-color: %2; border: 1px solid %3; border-radius: 5px;")
                                 .arg(DashboardPalette::TextPrimary.name(), DashboardPalette::CardBg.name(), DashboardPalette::BorderGuide.name()));
    row->addWidget(valueBox);
    if (valueLabelOut != nullptr) {
        *valueLabelOut = valueBox;
    }

    auto *unitLabel = makeLabel(unit);
    unitLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 9px;").arg(DashboardPalette::TextTertiary.name()));
    row->addWidget(unitLabel);
    layout->addLayout(row);

    layout->addWidget(makeProportionalBar(percent, DashboardPalette::Cyan, container));
    return container;
}

QWidget *RemoteControlPage::buildCmdVelPanel()
{
    auto *panel = panelFrame(QStringLiteral("callQueueCard"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(11, 8, 11, 8);
    layout->setSpacing(4);

    m_cmdVelTitleLabel = makeLabel(QStringLiteral("대기 · /cmd_vel"));
    m_cmdVelTitleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 9px; letter-spacing: 0.5px;").arg(DashboardPalette::Cyan.name()));
    layout->addWidget(m_cmdVelTitleLabel);

    auto *linearRow = new QHBoxLayout;
    linearRow->addWidget(makeLabel(QStringLiteral("linear.x"), QStringLiteral("metricCaption")));
    linearRow->addStretch();
    m_cmdVelLinearValueLabel = makeLabel(QStringLiteral("0.00 m/s"));
    m_cmdVelLinearValueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::TextPrimary.name()));
    linearRow->addWidget(m_cmdVelLinearValueLabel);
    layout->addLayout(linearRow);

    auto *angularRow = new QHBoxLayout;
    angularRow->addWidget(makeLabel(QStringLiteral("angular.z"), QStringLiteral("metricCaption")));
    angularRow->addStretch();
    m_cmdVelAngularValueLabel = makeLabel(QStringLiteral("0.00 rad/s"));
    m_cmdVelAngularValueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::TextPrimary.name()));
    angularRow->addWidget(m_cmdVelAngularValueLabel);
    layout->addLayout(angularRow);

    return panel;
}
