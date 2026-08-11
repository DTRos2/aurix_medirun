#include "livecontrolpage.h"

#include "../ros/navigationservice.h"
#include "../services/camerafeedprovider.h"
#include "../services/drivingrecordservice.h"
#include "../services/missioncontroller.h"
#include "../services/operationsoverviewservice.h"
#include "../services/sensortelemetryprovider.h"
#include "../services/telemetryprovider.h"
#include "../services/waypointrepository.h"
#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/mapviewwidget.h"
#include "../widgets/proportionalbar.h"

#include <QDate>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtMath>

namespace {

constexpr int kEventPollIntervalMs = 5000;

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

struct StepInfo
{
    QString label;
};

const QVector<StepInfo> kMissionSteps = {
    {QStringLiteral("호출 접수")},
    {QStringLiteral("출동 중")},
    {QStringLiteral("현장 도착")},
    {QStringLiteral("AED 반출")},
    {QStringLiteral("복귀·도킹")},
};

// MissionState는 "출동중/복귀중"만 구분하고 "현장 도착"·"AED 반출"은 별도 상태로
// 추적하지 않는다 (missiontypes.h 참고) — returningHome으로 전환되는 순간을
// "도착+반출 완료"로 근사한다. 정확한 하위 상태가 필요해지면 MissionState를
// 확장해야 한다.
QVector<QString> computeStepStates(const MissionSnapshot &snapshot)
{
    QVector<QString> states(kMissionSteps.size(), QStringLiteral("pending"));
    if (snapshot.missionId.isEmpty()) {
        return states;
    }
    states[0] = QStringLiteral("done");

    const bool arrivedOrLater = snapshot.returningHome || snapshot.state == MissionState::Completed;
    const bool dispatching = snapshot.state == MissionState::Starting || snapshot.state == MissionState::Navigating
        || snapshot.state == MissionState::Diverting;

    states[1] = arrivedOrLater ? QStringLiteral("done") : (dispatching ? QStringLiteral("active") : QStringLiteral("pending"));
    states[2] = arrivedOrLater ? QStringLiteral("done") : QStringLiteral("pending");
    states[3] = arrivedOrLater ? QStringLiteral("done") : QStringLiteral("pending");

    if (snapshot.state == MissionState::Completed) {
        states[4] = QStringLiteral("done");
    } else if (snapshot.returningHome) {
        states[4] = QStringLiteral("active");
    } else if (snapshot.state == MissionState::Failed || snapshot.state == MissionState::Cancelled) {
        // finishMission()이 returningHome을 리셋하기 직전 값을 저장해둔
        // arrivedBeforeEnd로, 실패/취소가 복귀 단계까지 가서 난 건지 그 전인
        // 출동/주행 중에 난 건지를 구분해 그 단계에 표시를 준다. 시스템이
        // 감지한 실패는 빨강("failed"), 운영자가 의도적으로 취소한 건 주황
        // ("cancelled")으로 구분한다.
        const QString marker = snapshot.state == MissionState::Failed ? QStringLiteral("failed") : QStringLiteral("cancelled");
        if (snapshot.arrivedBeforeEnd) {
            states[1] = states[2] = states[3] = QStringLiteral("done");
            states[4] = marker;
        } else {
            states[1] = marker;
        }
    }
    return states;
}

QColor colorForEventKind(const QString &kind)
{
    if (kind == QStringLiteral("amber")) {
        return DashboardPalette::Amber;
    }
    if (kind == QStringLiteral("red")) {
        return DashboardPalette::Red;
    }
    if (kind == QStringLiteral("cyan")) {
        return DashboardPalette::Cyan;
    }
    return DashboardPalette::TextMuted;
}

QWidget *buildEventRow(const OperationsEventRow &event)
{
    auto *rowWidget = new QWidget;
    auto *row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    auto *bar = new QFrame(rowWidget);
    bar->setFixedSize(3, 26);
    bar->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 1px;").arg(colorForEventKind(event.kind).name()));
    row->addWidget(bar);

    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(1);
    auto *typeLabel = makeLabel(QStringLiteral("%1 · %2").arg(event.occurredAt.toString(QStringLiteral("HH:mm:ss")), event.severityLabel));
    typeLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(colorForEventKind(event.kind).name()));
    textCol->addWidget(typeLabel);
    textCol->addWidget(makeLabel(event.message, QStringLiteral("metricCaption")));
    row->addLayout(textCol, 1);
    return rowWidget;
}

void clearVBoxLayout(QVBoxLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

} // namespace

LiveControlPage::LiveControlPage(QWidget *parent)
    : QWidget(parent)
    , m_mapView(nullptr)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildMissionStepper());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);

    m_mapView = new MapViewWidget(canvas);
    m_mapView->setMinimumHeight(250);
    m_mapView->setMode(MapViewMode::Live);
    m_mapView->setCaption(QStringLiteral("실시간 위치"));
    m_mapView->setRobotVisible(false);
    m_mapView->setOverlayChipsVisible(false);

    auto *mapCameraRow = new QHBoxLayout;
    mapCameraRow->setSpacing(8);
    mapCameraRow->addWidget(m_mapView, 1);
    mapCameraRow->addWidget(buildCameraPanel(), 1);

    auto *actionRow = buildActionRow();
    leftCol->addLayout(mapCameraRow, 1);
    leftCol->addWidget(actionRow);
    bodyRow->addLayout(leftCol, 7);

    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);
    rightCol->addWidget(buildRobotStatusPanel());
    rightCol->addWidget(buildEventStreamPanel(), 1);
    auto *callBanner = buildWaitingCallBanner();
    rightCol->addWidget(callBanner);
    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(200);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    layout->addLayout(bodyRow, 1);

    callBanner->setFixedHeight(actionRow->sizeHint().height());

    loadDefaultMap();
    refreshMissionStepper();
    refreshWaitingCallBanner();

    m_eventPollTimer = new QTimer(this);
    connect(m_eventPollTimer, &QTimer::timeout, this, &LiveControlPage::refreshEventStream);

    // 속도 라벨은 /odom 콜백 주기(수십 Hz)가 아니라 이 타이머 주기로만 갱신한다
    // (handleOdomVelocityUpdated는 값만 다듬어 저장, 화면 반영은 여기서).
    m_speedUpdateTimer = new QTimer(this);
    connect(m_speedUpdateTimer, &QTimer::timeout, this, &LiveControlPage::updateSpeedLabel);
    m_speedUpdateTimer->start(300);

    m_recordingElapsedTimer = new QTimer(this);
    connect(m_recordingElapsedTimer, &QTimer::timeout, this, &LiveControlPage::tickRecordingElapsed);
}

void LiveControlPage::setSession(const AppSession &session)
{
    m_session = session;
}

void LiveControlPage::setMissionController(MissionController *controller)
{
    if (m_missionController == controller) {
        return;
    }
    m_missionController = controller;
    if (m_missionController != nullptr) {
        connect(m_missionController, &MissionController::missionChanged, this, &LiveControlPage::refreshMissionStepper);
        connect(m_missionController, &MissionController::queueChanged, this, &LiveControlPage::refreshWaitingCallBanner);
        connect(m_missionController, &MissionController::missionChanged, this, &LiveControlPage::refreshMap);
    }
    refreshMissionStepper();
    refreshWaitingCallBanner();
}

void LiveControlPage::setNavigationService(NavigationService *service)
{
    if (m_navigationService == service) {
        return;
    }
    m_navigationService = service;
    if (m_navigationService != nullptr) {
        connect(m_navigationService, &NavigationService::amclPoseUpdated, this, &LiveControlPage::handleAmclPoseUpdated);
        connect(m_navigationService, &NavigationService::odomVelocityUpdated, this, &LiveControlPage::handleOdomVelocityUpdated);
        connect(m_navigationService, &NavigationService::nav2ConnectionChanged, this, &LiveControlPage::handleNav2ConnectionChanged);
    }
}

void LiveControlPage::setTelemetryProvider(TelemetryProvider *provider)
{
    if (m_telemetryProvider == provider) {
        return;
    }
    m_telemetryProvider = provider;
    if (m_telemetryProvider != nullptr) {
        connect(m_telemetryProvider, &TelemetryProvider::snapshotUpdated, this, &LiveControlPage::handleTelemetrySnapshot);
    }
}

void LiveControlPage::setSensorTelemetryProvider(SensorTelemetryProvider *provider)
{
    if (m_sensorTelemetryProvider == provider) {
        return;
    }
    m_sensorTelemetryProvider = provider;
    if (m_sensorTelemetryProvider != nullptr) {
        connect(m_sensorTelemetryProvider, &SensorTelemetryProvider::frontDistanceChanged, this, &LiveControlPage::handleFrontDistanceChanged);
        connect(m_sensorTelemetryProvider, &SensorTelemetryProvider::rearDistanceChanged, this, &LiveControlPage::handleRearDistanceChanged);
        connect(m_sensorTelemetryProvider, &SensorTelemetryProvider::payloadLoadedChanged, this, &LiveControlPage::handlePayloadLoadedChanged);
    }
}

void LiveControlPage::setCameraFeedProvider(CameraFeedProvider *provider)
{
    if (m_cameraFeedProvider == provider) {
        return;
    }
    m_cameraFeedProvider = provider;
    if (m_cameraFeedProvider != nullptr) {
        connect(m_cameraFeedProvider, &CameraFeedProvider::frameReady, this, &LiveControlPage::handleCameraFrameReady);
    }
}

void LiveControlPage::setWaypointRepository(WaypointRepository *repository)
{
    if (m_waypointRepository == repository) {
        return;
    }
    m_waypointRepository = repository;
    if (m_waypointRepository != nullptr) {
        connect(m_waypointRepository, &WaypointRepository::repositoryChanged, this, &LiveControlPage::refreshMap);
    }
    // 생성자 시점엔 아직 리포지토리가 주입되기 전이라 loadDefaultMap()이 항상
    // no-op이었다 — 실제로 리포지토리가 들어오는 지금 다시 시도해야 배경
    // 지도가 뜬다. 이미 로드돼 있으면(m_mapMeta.valid) 다시 읽지 않는다.
    if (!m_mapMeta.valid) {
        loadDefaultMap();
    }
    refreshMap();
}

void LiveControlPage::setOperationsOverviewService(OperationsOverviewService *service)
{
    m_operationsOverviewService = service;
    if (m_operationsOverviewService != nullptr) {
        refreshEventStream();
        if (m_eventPollTimer != nullptr && !m_eventPollTimer->isActive()) {
            m_eventPollTimer->start(kEventPollIntervalMs);
        }
    }
}

void LiveControlPage::setDrivingRecordService(DrivingRecordService *service)
{
    if (m_drivingRecordService == service) {
        return;
    }
    m_drivingRecordService = service;
    if (m_drivingRecordService != nullptr) {
        connect(m_drivingRecordService, &DrivingRecordService::recordingStateChanged, this, &LiveControlPage::handleRecordingStateChanged);
        connect(m_drivingRecordService, &DrivingRecordService::recordingPausedChanged, this, &LiveControlPage::handleRecordingPausedChanged);
        connect(m_drivingRecordService, &DrivingRecordService::recordingSaveFinished, this, &LiveControlPage::handleRecordingSaveFinished);
        handleRecordingStateChanged(m_drivingRecordService->isRecording());
        handleRecordingPausedChanged(m_drivingRecordService->isPaused());
    }
}

QWidget *LiveControlPage::buildMissionStepper()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(9);

    auto *infoRow = new QHBoxLayout;
    m_missionInfoLabel = makeLabel(QStringLiteral("대기 중"));
    m_missionInfoLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Cyan.name()));
    infoRow->addWidget(m_missionInfoLabel);
    infoRow->addStretch();
    m_missionMetaLabel = makeLabel(QString(), QStringLiteral("metricCaption"));
    infoRow->addWidget(m_missionMetaLabel);
    layout->addLayout(infoRow);

    auto *stepRow = new QHBoxLayout;
    stepRow->setSpacing(0);
    for (int i = 0; i < kMissionSteps.size(); ++i) {
        auto *col = new QVBoxLayout;
        col->setSpacing(4);
        auto *circle = new QLabel;
        circle->setProperty("stepState", QStringLiteral("pending"));
        circle->setFixedSize(18, 18);
        circle->setAlignment(Qt::AlignCenter);
        m_stepCircles.push_back(circle);
        col->addWidget(circle, 0, Qt::AlignHCenter);
        auto *stepLabel = makeLabel(kMissionSteps.at(i).label, QStringLiteral("metricCaption"));
        stepLabel->setAlignment(Qt::AlignHCenter);
        m_stepLabels.push_back(stepLabel);
        col->addWidget(stepLabel);
        stepRow->addLayout(col, 10);

        if (i < kMissionSteps.size() - 1) {
            auto *connector = new QFrame(panel);
            connector->setFixedHeight(2);
            connector->setObjectName(QStringLiteral("stepConnectorPending"));
            m_stepConnectors.push_back(connector);
            stepRow->addWidget(connector, 6, Qt::AlignVCenter);
        }
    }
    layout->addLayout(stepRow);
    return panel;
}

QWidget *LiveControlPage::buildActionRow()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    m_recordButton = new QPushButton(QStringLiteral("녹화 시작"));
    m_saveRecordingButton = new QPushButton(QStringLiteral("영상 저장"));
    m_saveRecordingButton->setEnabled(false);
    m_snapshotButton = new QPushButton(QStringLiteral("스냅샷"));
    auto *cancelButton = new QPushButton(QStringLiteral("미션 취소"));
    auto *homeButton = new QPushButton(QStringLiteral("홈 복귀"));

    for (QPushButton *button : {m_recordButton, m_saveRecordingButton, m_snapshotButton, cancelButton, homeButton}) {
        layout->addWidget(button);
    }

    connect(m_recordButton, &QPushButton::clicked, this, &LiveControlPage::handleRecordButtonClicked);
    connect(m_saveRecordingButton, &QPushButton::clicked, this, &LiveControlPage::handleSaveRecordingButtonClicked);
    connect(m_snapshotButton, &QPushButton::clicked, this, &LiveControlPage::handleSnapshotButtonClicked);
    connect(cancelButton, &QPushButton::clicked, this, &LiveControlPage::missionCancelRequested);
    connect(homeButton, &QPushButton::clicked, this, &LiveControlPage::returnHomeRequested);
    return row;
}

QWidget *LiveControlPage::buildRobotStatusPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("로봇 상태"), QStringLiteral("metricCaption")));

    m_batteryValueLabel = statValueLabel(layout, QStringLiteral("배터리"));
    m_batteryBarHost = panel;
    m_batteryBar = makeProportionalBar(0, DashboardPalette::TextMuted, panel);
    layout->addWidget(m_batteryBar);
    layout->addSpacing(4);
    m_payloadValueLabel = statValueLabel(layout, QStringLiteral("AED 적재"));
    m_obstacleValueLabel = statValueLabel(layout, QStringLiteral("전방 장애물"));
    m_rearObstacleValueLabel = statValueLabel(layout, QStringLiteral("후방 장애물"));
    m_nav2ValueLabel = statValueLabel(layout, QStringLiteral("Nav2"));
    m_speedValueLabel = statValueLabel(layout, QStringLiteral("속도"));
    return panel;
}

QWidget *LiveControlPage::buildEventStreamPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("이벤트 스트림"), QStringLiteral("metricCaption")));

    auto *streamContainer = new QWidget;
    m_eventStreamLayout = new QVBoxLayout(streamContainer);
    m_eventStreamLayout->setContentsMargins(0, 0, 0, 4);
    m_eventStreamLayout->setSpacing(6);

    // 미션 관제의 "오늘 처리 이력"과 동일한 패턴: 패널이 무한정 늘어나지
    // 않게 스크롤 영역에 담는다.
    auto *scrollArea = new QScrollArea(panel);
    scrollArea->setWidget(streamContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    layout->addWidget(scrollArea, 1);
    m_eventStreamLayout->addWidget(makeLabel(QStringLiteral("이벤트가 없습니다."), QStringLiteral("metricCaption")));

    return panel;
}

QWidget *LiveControlPage::buildWaitingCallBanner()
{
    auto *panel = panelFrame(QStringLiteral("callQueueCard"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(11, 0, 11, 0);
    layout->setSpacing(7);
    m_waitingCallLabel = makeLabel(QStringLiteral("대기 호출 0건"));
    m_waitingCallLabel->setWordWrap(false);
    m_waitingCallLabel->setStyleSheet(QStringLiteral("color: %1;").arg(DashboardPalette::Cyan.name()));
    layout->addWidget(m_waitingCallLabel);
    layout->addStretch();
    auto *linkLabel = makeLabel(QStringLiteral("미션 관제 →"), QStringLiteral("metricCaption"));
    linkLabel->setWordWrap(false);
    layout->addWidget(linkLabel);
    return panel;
}

QWidget *LiveControlPage::buildCameraPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    m_cameraViewLabel = new QLabel(QStringLiteral("🎥\n카메라 연결 대기 중"));
    m_cameraViewLabel->setAlignment(Qt::AlignCenter);
    m_cameraViewLabel->setStyleSheet(QStringLiteral("color: %1;").arg(DashboardPalette::TextMuted.name()));
    m_cameraViewLabel->setMinimumSize(1, 1);
    layout->addWidget(m_cameraViewLabel, 1);

    m_cameraTimelinePanel = new QWidget;
    auto *timelineLayout = new QVBoxLayout(m_cameraTimelinePanel);
    timelineLayout->setContentsMargins(9, 6, 9, 8);
    timelineLayout->setSpacing(6);
    timelineLayout->addWidget(makeProportionalBar(100, DashboardPalette::Red, m_cameraTimelinePanel));
    auto *controlsRow = new QHBoxLayout;
    controlsRow->setSpacing(9);
    controlsRow->addWidget(makeBadgeLabel(QStringLiteral("● REC"), QStringLiteral("red"), m_cameraTimelinePanel));
    m_recordingElapsedLabel = makeLabel(QStringLiteral("00:00"), QStringLiteral("metricCaption"));
    controlsRow->addWidget(m_recordingElapsedLabel);
    controlsRow->addStretch();
    m_pauseRecordingButton = new QPushButton(QStringLiteral("⏸"), m_cameraTimelinePanel);
    m_pauseRecordingButton->setObjectName(QStringLiteral("pauseChipButton"));
    m_pauseRecordingButton->setToolTip(QStringLiteral("일시정지"));
    connect(m_pauseRecordingButton, &QPushButton::clicked, this, &LiveControlPage::handlePauseRecordingButtonClicked);
    controlsRow->addWidget(m_pauseRecordingButton);
    timelineLayout->addLayout(controlsRow);
    m_cameraTimelinePanel->setVisible(false);
    layout->addWidget(m_cameraTimelinePanel);

    return panel;
}

void LiveControlPage::refreshMissionStepper()
{
    if (m_missionInfoLabel == nullptr) {
        return;
    }

    if (m_missionController == nullptr) {
        m_missionInfoLabel->setText(QStringLiteral("대기 중"));
        m_missionMetaLabel->clear();
    } else {
        const MissionSnapshot snapshot = m_missionController->missionSnapshot();
        if (snapshot.missionId.isEmpty()) {
            m_missionInfoLabel->setText(QStringLiteral("대기 중"));
            m_missionMetaLabel->clear();
        } else {
            m_missionInfoLabel->setText(QStringLiteral("%1 · %2").arg(snapshot.missionId, snapshot.destinationName));
            const QString elapsed = QStringLiteral("%1:%2")
                                         .arg(snapshot.elapsedSeconds / 60, 2, 10, QChar('0'))
                                         .arg(snapshot.elapsedSeconds % 60, 2, 10, QChar('0'));
            const QString remaining = snapshot.distanceRemainingMeters < 0.0
                ? QStringLiteral("--")
                : QString::number(snapshot.distanceRemainingMeters, 'f', 1);
            m_missionMetaLabel->setText(QStringLiteral("경과 %1 · 남은 %2m").arg(elapsed, remaining));
        }

        const QVector<QString> states = computeStepStates(snapshot);
        for (int i = 0; i < m_stepCircles.size() && i < states.size(); ++i) {
            QLabel *circle = m_stepCircles.at(i);
            if (states.at(i) == QStringLiteral("done")) {
                circle->setText(QStringLiteral("✓"));
            } else if (states.at(i) == QStringLiteral("failed")) {
                circle->setText(QStringLiteral("✕"));
            } else if (states.at(i) == QStringLiteral("cancelled")) {
                circle->setText(QStringLiteral("■"));
            } else {
                circle->setText(QString());
            }
            circle->setProperty("stepState", states.at(i));
            circle->style()->unpolish(circle);
            circle->style()->polish(circle);
        }
        for (int i = 0; i < m_stepConnectors.size(); ++i) {
            const bool done = i < states.size() - 1 && states.at(i) == QStringLiteral("done") && states.at(i + 1) != QStringLiteral("pending");
            QFrame *connector = m_stepConnectors.at(i);
            connector->setObjectName(done ? QStringLiteral("stepConnectorDone") : QStringLiteral("stepConnectorPending"));
            connector->style()->unpolish(connector);
            connector->style()->polish(connector);
        }
        return;
    }

    for (QLabel *circle : m_stepCircles) {
        circle->setText(QString());
        circle->setProperty("stepState", QStringLiteral("pending"));
        circle->style()->unpolish(circle);
        circle->style()->polish(circle);
    }
    for (QFrame *connector : m_stepConnectors) {
        connector->setObjectName(QStringLiteral("stepConnectorPending"));
        connector->style()->unpolish(connector);
        connector->style()->polish(connector);
    }
}

void LiveControlPage::refreshWaitingCallBanner()
{
    if (m_waitingCallLabel == nullptr) {
        return;
    }
    const int count = m_missionController == nullptr ? 0 : m_missionController->waitingQueue().size();
    m_waitingCallLabel->setText(QStringLiteral("대기 호출 %1건").arg(count));
}

void LiveControlPage::refreshEventStream()
{
    if (m_eventStreamLayout == nullptr || m_operationsOverviewService == nullptr) {
        return;
    }

    QVector<OperationsEventRow> rows;
    QString errorMessage;
    // 6건만 가져오면 항목이 패널 높이 안에 다 들어가버려서 스크롤이 생길
    // 일이 거의 없다 — "오늘 처리 이력"처럼 실제로 스크롤이 필요할 만큼
    // 더 넉넉히 가져온다.
    if (!m_operationsOverviewService->fetchRecentEvents(30, rows, errorMessage)) {
        return;
    }

    clearVBoxLayout(m_eventStreamLayout);
    if (rows.isEmpty()) {
        m_eventStreamLayout->addWidget(makeLabel(QStringLiteral("이벤트가 없습니다."), QStringLiteral("metricCaption")));
        return;
    }
    for (const OperationsEventRow &event : rows) {
        m_eventStreamLayout->addWidget(buildEventRow(event));
    }
    m_eventStreamLayout->addStretch();
}

void LiveControlPage::loadDefaultMap()
{
    if (m_waypointRepository == nullptr) {
        return;
    }
    const QString path = m_waypointRepository->workspaceRootPath() + QStringLiteral("/maps/last_class_map_modi.yaml");
    MapMeta meta;
    QString errorMessage;
    if (!loadMapYaml(path, meta, errorMessage) || !meta.valid) {
        return;
    }
    m_mapMeta = meta;
    m_mapView->setBackgroundImage(meta.image);
    refreshMapMarkers();
}

QPointF LiveControlPage::worldToNormalized(double x, double y) const
{
    const QPointF pixel = worldToPixel(QPointF(x, y), m_mapMeta);
    return QPointF(pixel.x() / m_mapMeta.width, pixel.y() / m_mapMeta.height);
}

void LiveControlPage::refreshMapMarkers()
{
    if (!m_mapMeta.valid || m_waypointRepository == nullptr) {
        return;
    }

    const WaypointMap waypoints = m_waypointRepository->waypoints();
    QVector<QPointF> waypointPoints;
    QPointF homePoint;
    bool hasHome = false;
    for (auto it = waypoints.cbegin(); it != waypoints.cend(); ++it) {
        const WaypointDef &def = it.value();
        const QPointF normalized = worldToNormalized(def.x, def.y);
        if (def.kind == WaypointMarkerKind::Home) {
            homePoint = normalized;
            hasHome = true;
        } else {
            waypointPoints.push_back(normalized);
        }
    }
    m_mapView->setWaypoints(waypointPoints);
    if (hasHome) {
        m_mapView->setHome(homePoint);
    }
}

void LiveControlPage::refreshMap()
{
    refreshMapMarkers();

    if (m_missionController == nullptr || !m_mapMeta.valid) {
        m_mapView->setDestination(QPointF(), false);
        return;
    }
    const MissionSnapshot snapshot = m_missionController->missionSnapshot();
    if (snapshot.missionId.isEmpty() || m_waypointRepository == nullptr) {
        m_mapView->setDestination(QPointF(), false);
        return;
    }

    // MissionSnapshot은 표시용 라벨(destinationName)만 갖고 있고 원본 waypoint id는
    // 없어서, 라벨(없으면 id) 일치로 역으로 찾는다. 라벨이 겹치면 먼저 찾은 것을 쓴다.
    const WaypointMap waypoints = m_waypointRepository->waypoints();
    for (auto it = waypoints.cbegin(); it != waypoints.cend(); ++it) {
        const WaypointDef &def = it.value();
        const QString displayName = def.label.isEmpty() ? def.name : def.label;
        if (displayName == snapshot.destinationName || def.name == snapshot.destinationName) {
            m_mapView->setDestination(worldToNormalized(def.x, def.y));
            return;
        }
    }
    m_mapView->setDestination(QPointF(), false);
}

void LiveControlPage::handleAmclPoseUpdated(double x, double y, double yawRad)
{
    if (!m_mapMeta.valid) {
        return;
    }
    m_mapView->setRobotVisible(true);
    m_mapView->setRobotPose(worldToNormalized(x, y), qRadiansToDegrees(yawRad));
    m_mapView->setRobotState(RobotMarkerState::Moving);
}

void LiveControlPage::handleOdomVelocityUpdated(double linearMps, double /*angularRadps*/)
{
    // 지수이동평균(alpha=0.25)으로 다듬는다 — 화면 갱신 자체는 m_speedUpdateTimer가 맡는다.
    m_lastLinearVel = m_lastLinearVel * 0.75 + linearMps * 0.25;
}

void LiveControlPage::updateSpeedLabel()
{
    if (m_speedValueLabel == nullptr) {
        return;
    }
    m_speedValueLabel->setText(QStringLiteral("%1 m/s").arg(m_lastLinearVel, 0, 'f', 2));
}

void LiveControlPage::handleTelemetrySnapshot(const TelemetrySnapshot &snapshot)
{
    m_lastBatteryPercent = snapshot.batteryPercent;
    if (m_batteryValueLabel == nullptr) {
        return;
    }

    const int displayPercent = snapshot.batteryPercent < 0 ? 0 : snapshot.batteryPercent;
    m_batteryValueLabel->setText(snapshot.batteryPercent < 0 ? QStringLiteral("--") : QStringLiteral("%1%").arg(snapshot.batteryPercent));
    const QColor color = displayPercent < 20 ? DashboardPalette::Red
        : displayPercent < 40 ? DashboardPalette::Amber
        : DashboardPalette::Green;
    setLabelColor(m_batteryValueLabel, color);

    // 값이 실제로 바뀔 때만 막대 위젯을 새로 만든다 — 매 스냅샷마다 다시 만들면
    // (배터리 %가 그대로여도) 위젯이 계속 교체되면서 깜빡여 보인다.
    if (displayPercent != m_lastDisplayedBatteryPercent) {
        m_lastDisplayedBatteryPercent = displayPercent;
        replaceBatteryBar(displayPercent);
    }
}

void LiveControlPage::replaceBatteryBar(int percent)
{
    if (m_batteryBarHost == nullptr || m_batteryBar == nullptr) {
        return;
    }
    const QColor color = percent < 20 ? DashboardPalette::Red : percent < 40 ? DashboardPalette::Amber : DashboardPalette::Green;
    auto *newBar = makeProportionalBar(percent, color, m_batteryBarHost);
    auto *layout = qobject_cast<QVBoxLayout *>(m_batteryBarHost->layout());
    if (layout != nullptr) {
        layout->replaceWidget(m_batteryBar, newBar);
    }
    delete m_batteryBar;
    m_batteryBar = newBar;
}

void LiveControlPage::handleFrontDistanceChanged(int centimeters)
{
    if (m_obstacleValueLabel == nullptr) {
        return;
    }
    m_obstacleValueLabel->setText(QStringLiteral("%1cm").arg(centimeters));
    setLabelColor(m_obstacleValueLabel, centimeters < 20 ? DashboardPalette::Red : DashboardPalette::TextPrimary);
}

void LiveControlPage::handleRearDistanceChanged(int centimeters)
{
    if (m_rearObstacleValueLabel == nullptr) {
        return;
    }
    m_rearObstacleValueLabel->setText(QStringLiteral("%1cm").arg(centimeters));
    setLabelColor(m_rearObstacleValueLabel, centimeters < 20 ? DashboardPalette::Red : DashboardPalette::TextPrimary);
}

void LiveControlPage::handlePayloadLoadedChanged(bool loaded)
{
    if (m_payloadValueLabel == nullptr) {
        return;
    }
    m_payloadValueLabel->setText(loaded ? QStringLiteral("적재됨") : QStringLiteral("미적재"));
    setLabelColor(m_payloadValueLabel, loaded ? DashboardPalette::Green : DashboardPalette::TextMuted);
}

void LiveControlPage::handleNav2ConnectionChanged(bool connected)
{
    m_lastNav2Connected = connected;
    updateNav2StatusLabel();
}

void LiveControlPage::updateNav2StatusLabel()
{
    if (m_nav2ValueLabel == nullptr) {
        return;
    }
    m_nav2ValueLabel->setText(m_lastNav2Connected ? QStringLiteral("연결됨") : QStringLiteral("연결 안 됨"));
    setLabelColor(m_nav2ValueLabel, m_lastNav2Connected ? DashboardPalette::Green : DashboardPalette::Red);
}

void LiveControlPage::handleCameraFrameReady(const QImage &frame)
{
    m_lastCameraFrame = frame;
    if (m_cameraViewLabel == nullptr || frame.isNull()) {
        return;
    }
    const QPixmap pixmap = QPixmap::fromImage(frame).scaled(
        m_cameraViewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_cameraViewLabel->setPixmap(pixmap);
}

void LiveControlPage::handleRecordingStateChanged(bool recording)
{
    if (m_saveRecordingButton != nullptr) {
        m_saveRecordingButton->setEnabled(recording);
    }
    if (m_pauseRecordingButton != nullptr) {
        m_pauseRecordingButton->setEnabled(recording);
        m_pauseRecordingButton->setText(QStringLiteral("⏸"));
        m_pauseRecordingButton->setToolTip(QStringLiteral("일시정지"));
        m_pauseRecordingButton->setObjectName(QStringLiteral("pauseChipButton"));
        m_pauseRecordingButton->style()->unpolish(m_pauseRecordingButton);
        m_pauseRecordingButton->style()->polish(m_pauseRecordingButton);
    }

    if (m_cameraTimelinePanel != nullptr) {
        m_cameraTimelinePanel->setVisible(recording);
    }
    if (recording) {
        m_recordingElapsedSeconds = 0;
        if (m_recordingElapsedLabel != nullptr) {
            m_recordingElapsedLabel->setText(QStringLiteral("00:00"));
        }
        if (m_recordingElapsedTimer != nullptr) {
            m_recordingElapsedTimer->start(1000);
        }
    } else if (m_recordingElapsedTimer != nullptr) {
        m_recordingElapsedTimer->stop();
    }
}

void LiveControlPage::handleRecordingPausedChanged(bool paused)
{
    if (m_pauseRecordingButton != nullptr) {
        m_pauseRecordingButton->setText(paused ? QStringLiteral("▶") : QStringLiteral("⏸"));
        m_pauseRecordingButton->setToolTip(paused ? QStringLiteral("재개") : QStringLiteral("일시정지"));
        m_pauseRecordingButton->setObjectName(paused ? QStringLiteral("resumeChipButton") : QStringLiteral("pauseChipButton"));
        m_pauseRecordingButton->style()->unpolish(m_pauseRecordingButton);
        m_pauseRecordingButton->style()->polish(m_pauseRecordingButton);
    }
    const bool recording = m_drivingRecordService != nullptr && m_drivingRecordService->isRecording();
    if (!recording || m_recordingElapsedTimer == nullptr) {
        return;
    }
    if (paused) {
        m_recordingElapsedTimer->stop();
    } else {
        m_recordingElapsedTimer->start(1000);
    }
}

void LiveControlPage::handleRecordingSaveFinished(bool ok, const QString &message)
{
    if (ok) {
        QMessageBox::information(
            this, QStringLiteral("영상 저장"), QStringLiteral("영상이 저장되었습니다.\n%1").arg(message));
    } else {
        QMessageBox::warning(
            this, QStringLiteral("영상 저장 실패"), QStringLiteral("영상 저장 중 문제가 발생했습니다.\n%1").arg(message));
    }
}

void LiveControlPage::tickRecordingElapsed()
{
    ++m_recordingElapsedSeconds;
    if (m_recordingElapsedLabel == nullptr) {
        return;
    }
    m_recordingElapsedLabel->setText(QStringLiteral("%1:%2")
                                          .arg(m_recordingElapsedSeconds / 60, 2, 10, QChar('0'))
                                          .arg(m_recordingElapsedSeconds % 60, 2, 10, QChar('0')));
}

void LiveControlPage::handleRecordButtonClicked()
{
    if (m_drivingRecordService != nullptr) {
        m_drivingRecordService->startRecording();
    }
}

void LiveControlPage::handleSaveRecordingButtonClicked()
{
    if (m_drivingRecordService == nullptr) {
        return;
    }
    const auto confirm = QMessageBox::question(
        this, QStringLiteral("영상 저장"),
        QStringLiteral("녹화를 종료하고 지금까지 촬영한 영상을 저장하시겠습니까?"));
    if (confirm != QMessageBox::Yes) {
        return;
    }
    m_drivingRecordService->stopRecording();
}

void LiveControlPage::handlePauseRecordingButtonClicked()
{
    if (m_drivingRecordService == nullptr) {
        return;
    }
    if (m_drivingRecordService->isPaused()) {
        m_drivingRecordService->resumeRecording();
    } else {
        m_drivingRecordService->pauseRecording();
    }
}

void LiveControlPage::handleSnapshotButtonClicked()
{
    if (m_lastCameraFrame.isNull() || m_operationsOverviewService == nullptr || m_waypointRepository == nullptr) {
        QMessageBox::warning(this, QStringLiteral("스냅샷 저장 실패"), QStringLiteral("카메라 영상이 없어 스냅샷을 저장할 수 없습니다."));
        return;
    }

    const QString snapshotDir = m_waypointRepository->workspaceRootPath() + QStringLiteral("/snapshots");
    QDir().mkpath(snapshotDir);
    const QString fileName = QStringLiteral("snapshot_%1_%2.png")
                                  .arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")),
                                       QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString fullPath = snapshotDir + QStringLiteral("/") + fileName;
    // PNG는 무손실이라 quality는 픽셀 품질이 아니라 zlib 압축 강도를 뜻하지만,
    // 100(최소 압축)으로 저장 시간을 줄이고 의도를 명확히 한다.
    if (!m_lastCameraFrame.save(fullPath, "PNG", 100)) {
        QMessageBox::warning(this, QStringLiteral("스냅샷 저장 실패"), QStringLiteral("파일 저장 중 오류가 발생했습니다.\n%1").arg(fullPath));
        return;
    }

    const QString missionId = m_missionController == nullptr ? QString() : m_missionController->missionSnapshot().missionId;
    const QString entityId = missionId.isEmpty() ? QStringLiteral("none") : missionId;
    QString errorMessage;
    if (!m_operationsOverviewService->recordFile(
            QStringLiteral("mission"), entityId, QStringLiteral("snapshot"), fullPath, fileName, QStringLiteral("image/png"),
            m_session.username, errorMessage)) {
        QMessageBox::warning(
            this, QStringLiteral("스냅샷 저장"),
            QStringLiteral("파일은 저장됐지만 DB 기록에 실패했습니다.\n%1\n%2").arg(fullPath, errorMessage));
        return;
    }

    QMessageBox::information(this, QStringLiteral("스냅샷 저장"), QStringLiteral("스냅샷이 저장되었습니다.\n%1").arg(fullPath));
}
