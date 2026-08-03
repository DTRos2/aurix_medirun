#include "remotecontrolpage.h"

#include "../ros/navigationservice.h"
#include "../services/camerafeedprovider.h"
#include "../services/mapfile.h"
#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/mapviewwidget.h"
#include "../widgets/proportionalbar.h"

#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
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
} // namespace

RemoteControlPage::RemoteControlPage(QWidget *parent)
    : QWidget(parent)
{
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
}

void RemoteControlPage::handleAmclPoseUpdated(double x, double y, double yawRad)
{
    if (!m_mapIsMain || !m_currentMapMeta.valid) {
        return;
    }

    m_mapView->setRobotPose(worldToNormalized(x, y), qRadiansToDegrees(yawRad));
    m_mapView->setRobotState(RobotMarkerState::Ready);
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

    auto *title = makeLabel(QStringLiteral("🎮  수동 제어 활성"));
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Red.name()));
    layout->addWidget(title);
    auto *caption = makeLabel(QStringLiteral("자율 목표 취소됨 · 제어권 qt / admin_lee"), QStringLiteral("metricCaption"));
    caption->setWordWrap(false);
    layout->addWidget(caption);
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

    m_cameraBottomRow = new QWidget(panel);
    auto *bottomRow = new QHBoxLayout(m_cameraBottomRow);
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->addStretch();
    auto *front = makeLabel(QStringLiteral("전방 0.42m"));
    front->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; padding: 3px 9px; border: 1px solid #2a4a66; border-radius: 6px;").arg(DashboardPalette::Green.name()));
    bottomRow->addWidget(front);
    auto *left = makeLabel(QStringLiteral("좌 0.88m"));
    left->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; padding: 3px 9px; border: 1px solid #2a4a66; border-radius: 6px;").arg(DashboardPalette::TextSecondary.name()));
    bottomRow->addWidget(left);
    auto *right = makeLabel(QStringLiteral("우 1.14m"));
    right->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; padding: 3px 9px; border: 1px solid #2a4a66; border-radius: 6px;").arg(DashboardPalette::TextSecondary.name()));
    bottomRow->addWidget(right);
    bottomRow->addStretch();
    layout->addWidget(m_cameraBottomRow);
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
    m_mapView->setRobotPose(QPointF(0.35, 0.65), 0.0);
    m_mapView->setRobotState(RobotMarkerState::Warning);
    m_mapView->setHome(QPointF(0.78, 0.3));
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

    auto *icon = makeLabel(QStringLiteral("⚠"));
    icon->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Amber.name()));
    layout->addWidget(icon);

    auto *text = makeLabel(QStringLiteral("로봇의 실제 위치와 지도 위 마커가 일치하는지 확인하세요 · 다르면 초기 위치 설정"));
    text->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Amber.name()));
    layout->addWidget(text, 1);

    return panel;
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

void RemoteControlPage::handleLoadMap()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("지도 불러오기 (nav2 map_server YAML)"), QString(),
        QStringLiteral("맵 YAML (*.yaml *.yml)"));
    if (path.isEmpty()) {
        return;
    }

    MapMeta meta;
    QString errorMessage;
    if (!loadMapYaml(path, meta, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("지도 불러오기 실패"), errorMessage);
        return;
    }

    m_currentMapMeta = meta;
    m_mapView->setBackgroundImage(meta.image);
    updateInitialPoseButtonEnabled();

    if (!errorMessage.isEmpty()) {
        // 회전(origin theta) 경고 등 — 실패는 아니므로 로드는 이미 적용된 상태.
        QMessageBox::information(this, QStringLiteral("지도 불러오기"), errorMessage);
    }
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
        // 배지/거리칩 오버레이를 감춰서 축소된 높이 대부분을 실제 영상에 내준다
        // (100px로는 오버레이만 겨우 보이고 영상은 안 보이던 문제 수정).
        m_cameraTopRow->setVisible(false);
        m_cameraBottomRow->setVisible(false);
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
        m_cameraBottomRow->setVisible(true);
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

    layout->addWidget(directionButton(QStringLiteral("▲")), 0, 1);
    layout->addWidget(directionButton(QStringLiteral("↺")), 1, 0);
    layout->addWidget(directionButton(QStringLiteral("정지"), true), 1, 1);
    layout->addWidget(directionButton(QStringLiteral("↻")), 1, 2);
    layout->addWidget(directionButton(QStringLiteral("▼")), 2, 1);
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
    auto *fineButton = new QPushButton(QStringLiteral("미세"));
    fineButton->setObjectName(QStringLiteral("ghostChipButton"));
    auto *normalButton = new QPushButton(QStringLiteral("보통"));
    normalButton->setObjectName(QStringLiteral("cyanChipButton"));
    auto *fastButton = new QPushButton(QStringLiteral("빠름"));
    fastButton->setObjectName(QStringLiteral("ghostChipButton"));
    presetRow->addWidget(fineButton, 1);
    presetRow->addWidget(normalButton, 1);
    presetRow->addWidget(fastButton, 1);
    layout->addLayout(presetRow);

    layout->addWidget(buildSpeedAxisRow(QStringLiteral("▲ 직진"), QStringLiteral("0.12"), QStringLiteral("m/s"), 55));
    layout->addWidget(buildSpeedAxisRow(QStringLiteral("↻ 회전"), QStringLiteral("0.80"), QStringLiteral("rad/s"), 28));

    return panel;
}

QWidget *RemoteControlPage::buildSpeedAxisRow(const QString &label, const QString &value, const QString &unit, int percent)
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

    auto *valueBox = makeLabel(value);
    valueBox->setAlignment(Qt::AlignCenter);
    valueBox->setFixedSize(50, 22);
    valueBox->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background-color: %2; border: 1px solid %3; border-radius: 5px;")
                                 .arg(DashboardPalette::TextPrimary.name(), DashboardPalette::CardBg.name(), DashboardPalette::BorderGuide.name()));
    row->addWidget(valueBox);

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

    auto *title = makeLabel(QStringLiteral("발행 중 · /cmd_vel"));
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 9px; letter-spacing: 0.5px;").arg(DashboardPalette::Cyan.name()));
    layout->addWidget(title);

    auto *linearRow = new QHBoxLayout;
    linearRow->addWidget(makeLabel(QStringLiteral("linear.x"), QStringLiteral("metricCaption")));
    linearRow->addStretch();
    auto *linearValue = makeLabel(QStringLiteral("0.00 m/s"));
    linearValue->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::TextPrimary.name()));
    linearRow->addWidget(linearValue);
    layout->addLayout(linearRow);

    auto *angularRow = new QHBoxLayout;
    angularRow->addWidget(makeLabel(QStringLiteral("angular.z"), QStringLiteral("metricCaption")));
    angularRow->addStretch();
    auto *angularValue = makeLabel(QStringLiteral("0.00 rad/s"));
    angularValue->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::TextPrimary.name()));
    angularRow->addWidget(angularValue);
    layout->addLayout(angularRow);

    return panel;
}
