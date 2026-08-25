#include "missioncontroller.h"

#include "../ros/navigationservice.h"
#include "missionrosinterface.h"
#include "missionstorageservice.h"
#include "operationsoverviewservice.h"
#include "robotservice.h"
#include "waypointrepository.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTimer>
#include <QTime>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <QtMath>

namespace {

QString historyId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// observedButtonIds()를 앱 재시작 후에도 남기기 위한 로컬 설정 키 — 매번
// 실제 버튼을 다시 눌러야만 웨이포인트관리 페이지 드롭다운에 뜨는 걸 막는다
// (main.cpp에서 QApplication에 조직/앱 이름이 이미 설정돼 있어 QSettings()를
// 인자 없이 그대로 쓸 수 있다).
constexpr auto kObservedButtonIdsSettingsKey = "mission/observedCallButtonIds";

QString resultText(const QString &resultCode)
{
    if (resultCode == QStringLiteral("SUCCEEDED")) {
        return QStringLiteral("성공");
    }
    if (resultCode == QStringLiteral("CANCELED")) {
        return QStringLiteral("취소");
    }
    return QStringLiteral("실패");
}

} // namespace

MissionController::MissionController(const AppSession &session,
                                     NavigationService *navigationService,
                                     WaypointRepository *waypointRepository,
                                     MissionStorageService *storageService,
                                     MissionRosInterface *rosInterface,
                                     RobotService *robotService,
                                     OperationsOverviewService *overviewService,
                                     QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_navigationService(navigationService)
    , m_waypointRepository(waypointRepository)
    , m_storageService(storageService)
    , m_rosInterface(rosInterface)
    , m_robotService(robotService)
    , m_overviewService(overviewService)
{
    m_storageService->setSessionContext(m_session);
    m_queueItems = m_storageService->loadQueue();
    m_historyEntries = m_storageService->loadTodayHistory();
    m_observedButtonIds = QSettings().value(QLatin1String(kObservedButtonIdsSettingsKey)).toStringList();
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &MissionController::updateElapsed);
    m_elapsedTimer->start();

    // 복귀·도킹 완료(Completed) 직후 startNextQueueIfPossible()이 바로
    // resetToIdleSnapshot()을 불러버리면 실시간 관제 스텝퍼의 "완료" 체크마크가
    // 화면에 그려질 새도 없이 지워진다(2026-08-24). 그래서 Completed일 때만
    // startNextQueueIfPossible() 호출을 이 타이머로 30초 미룬다 — 그동안
    // 완료 체크마크가 보이고, 새 호출이 들어오면 enqueueCall()이 어차피
    // 즉시 startNextQueueIfPossible()을 부르니 대기할 필요 없이 바로 처리된다.
    m_completionResetTimer = new QTimer(this);
    m_completionResetTimer->setSingleShot(true);
    connect(m_completionResetTimer, &QTimer::timeout, this, &MissionController::startNextQueueIfPossible);

    reconcileOrphanedActiveMission();

    connect(m_waypointRepository, &WaypointRepository::repositoryChanged, this, &MissionController::handleWaypointRepositoryChanged);

    connect(m_navigationService, &NavigationService::navigationGoalAccepted, this, &MissionController::handleNavigationAccepted);
    connect(m_navigationService, &NavigationService::navigationGoalRejected, this, &MissionController::handleNavigationRejected);
    connect(m_navigationService, &NavigationService::navigationResult, this, &MissionController::handleNavigationResult);
    connect(m_navigationService, &NavigationService::nav2ConnectionChanged, this, &MissionController::handleNav2ConnectionChanged);
    connect(m_navigationService, &NavigationService::feedbackDistanceRemaining, this, &MissionController::handleNavigationFeedback);
    connect(m_navigationService, &NavigationService::amclPoseUpdated, this, &MissionController::handleAmclPoseUpdated);
    // Nav2가 이 컨트롤러보다 먼저 연결된 상태였다면 nav2ConnectionChanged를
    // 놓쳤을 수 있어 현재 상태로 한 번 동기화한다. amcl_pose는
    // TRANSIENT_LOCAL이라 amclPoseUpdated도 같은 문제가 있다.
    handleNav2ConnectionChanged(m_navigationService->isNav2Connected());
    if (double x, y, yaw; m_navigationService->lastKnownAmclPose(x, y, yaw)) {
        handleAmclPoseUpdated(x, y, yaw);
    }

    connect(m_rosInterface, &MissionRosInterface::dispatchRequested, this, &MissionController::handleDispatchRequested);
    connect(m_rosInterface, &MissionRosInterface::missionHeartbeat, this, &MissionController::handleRosHeartbeat);
    connect(m_rosInterface, &MissionRosInterface::communicationRetryFinished, this, &MissionController::handleRosRetryFinished);
    connect(m_rosInterface, &MissionRosInterface::dockResultReceived, this, &MissionController::handleDockResult);
    connect(m_rosInterface, &MissionRosInterface::piReturnStatusReceived, this, &MissionController::handlePiReturnStatus);

    updateConnectionState();
    startNextQueueIfPossible();
}

MissionSnapshot MissionController::missionSnapshot() const
{
    return m_snapshot;
}

QVector<MissionQueueItem> MissionController::waitingQueue() const
{
    return m_queueItems;
}

QVector<MissionHistoryEntry> MissionController::todayHistory() const
{
    // DB(system_event 등)에는 전부 남지만, GUI에는 최근 24시간 안의 항목만
    // 보여준다 — 자정 기준 "오늘"이 아니라 지금 시각 기준 rolling 24시간이라
    // GUI를 며칠씩 켜놔도 표시 목록이 무한정 쌓이지 않는다.
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-24 * 60 * 60);
    QVector<MissionHistoryEntry> recent;
    recent.reserve(m_historyEntries.size());
    for (const MissionHistoryEntry &entry : m_historyEntries) {
        if (entry.occurredAt >= cutoff) {
            recent.append(entry);
        }
    }
    return recent;
}

QString MissionController::queueSummaryText() const
{
    const int count = std::count_if(m_queueItems.cbegin(), m_queueItems.cend(), [](const MissionQueueItem &item) {
        return item.status == MissionQueueStatus::Waiting || item.status == MissionQueueStatus::Active;
    });
    return QStringLiteral("대기 호출 큐 · %1건").arg(count);
}

QString MissionController::anomalyTitle() const
{
    if (!m_nav2Connected || !m_rosConnected) {
        return QStringLiteral("⚠ 이상 상황 · 통신 확인 필요");
    }
    if (m_snapshot.state == MissionState::Failed) {
        return QStringLiteral("⚠ 이상 상황 · 미션 실패");
    }
    return QStringLiteral("⚠ 이상 상황 · 모니터링 중");
}

QString MissionController::anomalyDetail() const
{
    if (!m_nav2Connected || !m_rosConnected) {
        return QStringLiteral("ROS2 graph 또는 Nav2 연결이 불안정합니다. 재시도로 graph와 action 연결을 다시 확인하세요.");
    }
    if (!m_snapshot.failureReason.isEmpty()) {
        return m_snapshot.failureReason;
    }
    return QStringLiteral("현재 연결은 정상입니다. 미션/큐 상태를 계속 감시합니다.");
}

QString MissionController::anomalyMeta() const
{
    const QString missionId = m_snapshot.missionId.isEmpty() ? QStringLiteral("-") : m_snapshot.missionId;
    const QString location = m_snapshot.destinationName.isEmpty() ? QStringLiteral("-") : m_snapshot.destinationName;
    return QStringLiteral("%1 · 미션 %2 · %3").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), missionId, location);
}

bool MissionController::rosConnected() const
{
    return m_rosConnected;
}

bool MissionController::nav2Connected() const
{
    return m_nav2Connected;
}

bool MissionController::retryInProgress() const
{
    return m_retryInProgress;
}

QStringList MissionController::observedButtonIds() const
{
    return m_observedButtonIds;
}

void MissionController::announceTestDrive(const QString &waypointLabel)
{
    m_rosInterface->publishAudioAnnouncement(waypointLabel.isEmpty()
        ? QStringLiteral("지정한 위치로 이동합니다")
        : QStringLiteral("%1(으)로 이동합니다").arg(waypointLabel));
}

void MissionController::requestRetry()
{
    if (m_retryInProgress) {
        return;
    }
    m_retryInProgress = true;
    emit retryStateChanged();
    addHistory(QStringLiteral("ROS_RETRY_STARTED"), QString(), QString(), m_snapshot.missionId, m_snapshot.queueId,
               QStringLiteral("STARTED"), QStringLiteral("ROS2 재연결 시도 시작"));
    log(QStringLiteral("ROS2"), QStringLiteral("retry requested"));
    m_navigationService->retryConnections();
    m_rosInterface->retryCommunication();
}

void MissionController::requestReturnHome()
{
    // 미션 취소/홈 복귀는 조종 권한(리더 선출)과 무관하게 어느 창에서든
    // 요청할 수 있다 — cancelGoal()이 로컬 goal 핸들이 없어도 Nav2 액션
    // 서버에 전체 취소를 요청하는 "orphan-safe" 경로를 타므로, 팀원이 다른
    // 창에서 눌러도 안전하게 동작한다(nav2navigationservice.cpp 참고).
    if (beginReturnHomeFlow(QStringLiteral("운영자 홈 복귀 요청"))) {
        m_rosInterface->publishMissionCommand(QStringLiteral("return_home"), m_snapshot.missionId, m_snapshot.queueId, QString());
    }
}

void MissionController::requestMissionCancel()
{
    if (m_snapshot.state == MissionState::Idle) {
        addHistory(QStringLiteral("MISSION_CANCEL_IGNORED"), QString(), QString(), QString(), QString(),
                   QStringLiteral("IGNORED"), QStringLiteral("취소할 진행 중 미션이 없습니다."));
        emit historyChanged();
        return;
    }
    if (m_snapshot.state == MissionState::Cancelling || m_snapshot.state == MissionState::Diverting) {
        return;
    }

    // 이미 호출구역에 도착해 AED 반출 대기 중이면 "그 자리에서 정지"는 의미가
    // 없다(로봇이 이미 멈춰 있음) — 운영자 확인(2026-08-19)에 따라 즉시 홈
    // 복귀로 처리한다. 취소할 Nav2 goal 자체가 없으므로 beginReturnHomeFlow의
    // Arrived 분기(cancelGoal 없이 바로 홈으로 출발)를 그대로 탄다.
    if (m_snapshot.state == MissionState::Arrived) {
        if (beginReturnHomeFlow(QStringLiteral("운영자 미션 취소 요청 — 호출구역 도착 후라 즉시 홈 복귀로 처리"))) {
            m_rosInterface->publishMissionCommand(QStringLiteral("return_home"), m_snapshot.missionId, m_snapshot.queueId, QString());
        }
        return;
    }

    if (m_snapshot.state == MissionState::Parking) {
        // 전면주차는 Nav2 goal이 아니라 docking_manager가 /cmd_vel을 직접
        // 제어하는 중이라 cancelGoal()로는 못 멈춘다 — dock_request(false)로
        // 정지시키고, 결과는 handleDockResult가 "취소" 사유를 보고 Cancelled로
        // 마무리한다.
        m_pendingReason = QStringLiteral("운영자 미션 취소 요청 — 전면주차 중단");
        addHistory(QStringLiteral("MISSION_CANCEL_REQUESTED"), QString(), m_snapshot.destinationName, m_snapshot.missionId,
                   m_snapshot.queueId, QStringLiteral("REQUESTED"), m_pendingReason);
        emit historyChanged();
        m_rosInterface->cancelDocking();
        return;
    }

    // 홈 복귀 없이 그 자리에서 정지 + 진행 중이던 호출(큐 항목)은 삭제한다.
    // 실제 종료 처리(Cancelled 상태 전환, 큐 삭제)는 Nav2가 취소를 확인해 준
    // 뒤 handleNavigationResult()의 CANCELED 처리에서 수행한다.
    m_pendingAction = PendingAction::None;
    m_pendingReason = QStringLiteral("운영자 미션 취소 요청 — 현재 위치에서 정지");
    setMissionState(MissionState::Cancelling);
    addHistory(QStringLiteral("MISSION_CANCEL_REQUESTED"), QString(), m_snapshot.destinationName, m_snapshot.missionId, m_snapshot.queueId,
               QStringLiteral("REQUESTED"), m_pendingReason);
    emit historyChanged();
    m_navigationService->cancelGoal();
    m_rosInterface->publishMissionCommand(QStringLiteral("cancel"), m_snapshot.missionId, m_snapshot.queueId, QString());
}

void MissionController::requestQueueCancel(const QString &queueId)
{
    const int index = queueIndex(queueId);
    if (index < 0) {
        return;
    }
    if (m_queueItems.at(index).status == MissionQueueStatus::Active) {
        requestMissionCancel();
        return;
    }

    m_queueItems[index].status = MissionQueueStatus::Cancelled;
    const MissionQueueItem item = m_queueItems.at(index);
    saveQueue();
    m_queueItems.removeAt(index);
    addHistory(QStringLiteral("QUEUE_CANCELLED"), item.waypointId, item.waypointName, item.missionId, item.queueId,
               QStringLiteral("CANCELLED"), QStringLiteral("대기 호출 큐 취소"));
    log(QStringLiteral("QUEUE"), QStringLiteral("cancelled queue_id=%1 waypoint_id=%2").arg(item.queueId, item.waypointId));
    emit queueChanged();
    emit historyChanged();
}

void MissionController::requestQueueDivert(const QString &queueId)
{
    const int index = queueIndex(queueId);
    if (index < 0) {
        return;
    }
    if (m_snapshot.state == MissionState::Idle) {
        startMissionForQueue(queueId, false);
        return;
    }
    // Arrived(호출구역 도착 후 대기)는 취소할 활성 Nav2 goal이 없어 여기 아래의
    // cancelGoal()+CANCELED 대기 흐름을 못 탄다 — 도착 후 전환은 막고, 운영자는
    // 먼저 "복귀"나 "취소"(둘 다 즉시 홈 복귀로 처리됨)로 이 미션부터 끝내야 한다.
    if (m_snapshot.returningHome || m_snapshot.state == MissionState::Arrived
        || m_snapshot.state == MissionState::Cancelling || m_snapshot.state == MissionState::Diverting) {
        return;
    }

    m_pendingAction = PendingAction::Divert;
    m_pendingDivertQueueId = queueId;
    setMissionState(MissionState::Diverting);
    m_pendingReason = QStringLiteral("확인 전환");
    log(QStringLiteral("MISSION"), QStringLiteral("divert start queue_id=%1 mission_id=%2").arg(queueId, m_snapshot.missionId));
    addHistory(QStringLiteral("MISSION_DIVERT_REQUESTED"), m_queueItems.at(index).waypointId, m_queueItems.at(index).waypointName,
               m_snapshot.missionId, queueId, QStringLiteral("REQUESTED"), QStringLiteral("확인 전환 시작"));
    m_navigationService->cancelGoal();
}

void MissionController::handleDispatchRequested(const QString &buttonId,
                                                const QString &requestSource,
                                                const QString &missionId,
                                                const QDateTime &requestedAt)
{
    enqueueCall(buttonId, requestSource, missionId, requestedAt);
}

void MissionController::handleWaypointRepositoryChanged()
{
    if (!m_snapshot.missionId.isEmpty()) {
        m_snapshot.currentWaypointName = m_robotPoseValid ? nearestWaypointName(m_lastRobotX, m_lastRobotY) : m_snapshot.currentWaypointName;
        emit missionChanged();
    }
}

void MissionController::handleNavigationAccepted()
{
    m_goalAccepted = true;
    if (m_snapshot.state == MissionState::Starting) {
        setMissionState(MissionState::Navigating);
    }
    log(QStringLiteral("NAV2"), QStringLiteral("goal accepted mission_id=%1 queue_id=%2").arg(m_snapshot.missionId, m_snapshot.queueId));
}

void MissionController::handleNavigationRejected(const QString &reason)
{
    m_goalAccepted = false;
    const QString detail = reason.isEmpty() ? QStringLiteral("goal rejected") : reason;
    log(QStringLiteral("NAV2"), QStringLiteral("goal rejected mission_id=%1 detail=%2").arg(m_snapshot.missionId, detail));
    finishMission(MissionState::Failed, detail, QStringLiteral("NAV_GOAL_REJECTED"), QStringLiteral("FAILED"));
}

void MissionController::handleNavigationResult(const QString &resultCode, const QString &detail)
{
    log(QStringLiteral("NAV2"),
        QStringLiteral("goal result mission_id=%1 queue_id=%2 result=%3 detail=%4")
            .arg(m_snapshot.missionId, m_snapshot.queueId, resultCode, detail));

    if (resultCode == QStringLiteral("CANCELED")) {
        if (m_pendingAction == PendingAction::Divert && !m_pendingDivertQueueId.isEmpty()) {
            if (MissionQueueItem *item = activeQueueItem()) {
                item->status = MissionQueueStatus::Diverted;
            }
            addHistory(QStringLiteral("MISSION_DIVERTED"), QString(), m_snapshot.destinationName, m_snapshot.missionId, m_snapshot.queueId,
                       QStringLiteral("DIVERTED"), QStringLiteral("기존 미션을 중단하고 새 목적지로 전환"));
            const QString divertedQueueId = m_pendingDivertQueueId;
            m_pendingAction = PendingAction::None;
            m_pendingDivertQueueId.clear();
            startMissionForQueue(divertedQueueId, true);
            return;
        }
        if (m_pendingAction == PendingAction::ReturnHome) {
            dispatchPendingReturnHome();
            return;
        }

        // 미션 취소(그 자리에서 정지) — Nav2가 취소를 확인해준 시점. finishMission이
        // activeQueueItem()으로 이 항목을 찾아 Cancelled로 표시하고 DB에도 반영한다
        // (Completed/Failed와 동일한 패턴). 예전에는 여기서 removeQueueItem()을 먼저
        // 불러 큐에서 지워버린 뒤 finishMission을 호출했는데, 그러면 activeQueueItem()이
        // 이미 사라진 항목을 못 찾아 상태를 못 바꾸고 DB에는 마지막으로 저장된
        // "navigating"이 그대로 남는 버그가 있었다 — 취소해도 DB만 계속 주행 중으로
        // 보였다. Waiting/Active만 화면에 보여주는 필터(queueSummaryText 등) 덕분에
        // 큐에서 안 지워도 목록에서는 정상적으로 사라진다.
        // 다음 대기 호출로 바로 주행하지 않고, 운영자에게 확인을 받는다.
        finishMission(MissionState::Cancelled, m_pendingReason.isEmpty() ? QStringLiteral("운영자 미션 취소 요청") : m_pendingReason,
                      QStringLiteral("MISSION_CANCELLED"), QStringLiteral("CANCELLED"), true);
        return;
    }

    if (resultCode == QStringLiteral("SUCCEEDED")) {
        // 홈 복귀 요청으로 cancelGoal()을 보내고 CANCELED를 기다리는 중(state
        // == Cancelling)이었는데, 취소 확인보다 기존 목표의 SUCCEEDED가 먼저
        // 도착하는 경쟁 상태가 있었다 — 이 경우 아래로 흘러가 beginArrivedWait()를
        // 타면서 홈 복귀 요청 자체가 조용히 무시됐다("웨이포인트 도착 후 홈
        // 복귀 버튼을 눌러도 반응 없음" 버그). CANCELED로 오든 SUCCEEDED로
        // 오든 대기 중이던 홈 복귀 요청이 항상 이기도록 여기서도 처리한다.
        if (m_snapshot.state != MissionState::ReturningHome && m_pendingAction == PendingAction::ReturnHome) {
            dispatchPendingReturnHome();
            return;
        }
        if (m_snapshot.state == MissionState::ReturningHome) {
            if (m_pendingReason.contains(QStringLiteral("취소"))) {
                finishMission(MissionState::Cancelled, m_pendingReason, QStringLiteral("RETURN_HOME_COMPLETED"), QStringLiteral("SUCCEEDED"));
                return;
            }
            // 홈(또는 HOME_PRE_DOCK) 도착 — 여기서 바로 끝내지 않고 항상 ArUco
            // 전면주차로 이어간다. docking_manager는 simulate_docking 파라미터가
            // true(기본값)면 카메라/마커 없이도 잠깐 대기 후 SUCCEEDED를 돌려주므로,
            // 카메라가 없는 로봇도 안전하게 그대로 완료된다 — false로 바꾼
            // 로봇만 실제 ArUco 인식을 수행한다. /medirun/vision/dock_result가
            // 오면 handleDockResult에서 최종 마무리한다.
            addHistory(QStringLiteral("PARKING_STARTED"), QString(), m_snapshot.destinationName, m_snapshot.missionId,
                       m_snapshot.queueId, QStringLiteral("STARTED"), QStringLiteral("홈 도착 — ArUco 전면주차 시작"));
            setMissionState(MissionState::Parking);
            emit historyChanged();
            m_rosInterface->requestDocking();
            return;
        }
        // 호출구역 도착 — 여기서 바로 미션을 끝내지 않고 AED 반출 대기로
        // 전환한다(SR-F-010/TC-12: 반출 감지 15초 후 자동 복귀, 미반출 시
        // 60초 안내 반복 후 180초 경과 시 강제 복귀).
        beginArrivedWait();
        return;
    }

    finishMission(MissionState::Failed, detail, QStringLiteral("MISSION_FAILED"), QStringLiteral("FAILED"));
}

void MissionController::handleNav2ConnectionChanged(bool connected)
{
    m_nav2Connected = connected;
    updateConnectionState();
    emit connectionStateChanged();
}

void MissionController::handleNavigationFeedback(double metersRemaining)
{
    m_snapshot.distanceRemainingMeters = metersRemaining;
    updateProgress();
    emit missionChanged();
}

void MissionController::handleAmclPoseUpdated(double x, double y, double)
{
    m_lastRobotX = x;
    m_lastRobotY = y;
    m_robotPoseValid = true;
    if (!m_snapshot.missionId.isEmpty()) {
        m_snapshot.currentWaypointName = nearestWaypointName(x, y);
        updateProgress();
        emit missionChanged();
    }
}

void MissionController::handleRosHeartbeat(const QString &source, const QDateTime &receivedAt)
{
    m_lastRosHeartbeat = receivedAt;
    m_lastRosHeartbeatSource = source;
    updateConnectionState();
    emit connectionStateChanged();
}

void MissionController::handleRosRetryFinished(bool success, const QString &detail)
{
    const bool finalSuccess = success && m_nav2Connected;
    m_retryInProgress = false;
    addHistory(finalSuccess ? QStringLiteral("ROS_RETRY_SUCCEEDED") : QStringLiteral("ROS_RETRY_FAILED"),
               QString(), QString(), m_snapshot.missionId, m_snapshot.queueId,
               finalSuccess ? QStringLiteral("SUCCEEDED") : QStringLiteral("FAILED"), detail);
    log(QStringLiteral("ROS2"), QStringLiteral("retry result success=%1 detail=%2").arg(finalSuccess).arg(detail));
    updateConnectionState();
    emit retryStateChanged();
    emit historyChanged();
}

void MissionController::updateElapsed()
{
    if (m_snapshot.startedAt.isValid() && m_snapshot.state != MissionState::Idle) {
        m_snapshot.elapsedSeconds = m_snapshot.startedAt.secsTo(QDateTime::currentDateTime());
        emit missionChanged();
    }
    updateConnectionState();
}

void MissionController::enqueueCall(const QString &buttonId,
                                    const QString &requestSource,
                                    const QString &missionId,
                                    const QDateTime &requestedAt)
{
    // 등록 성공/실패와 무관하게, 실제 하드웨어가 보낸 raw button_id를 전부
    // 기록해둔다 — 웨이포인트관리 페이지가 콜존 등록 시 "감지된 콜존 ID"
    // 드롭다운으로 보여준다. 최근 감지된 것이 앞에 오게 유지.
    m_observedButtonIds.removeAll(buttonId);
    m_observedButtonIds.prepend(buttonId);
    QSettings().setValue(QLatin1String(kObservedButtonIdsSettingsKey), m_observedButtonIds);
    emit observedButtonIdsChanged();

    for (const MissionQueueItem &existing : std::as_const(m_queueItems)) {
        if (existing.waypointId == buttonId
            && (existing.status == MissionQueueStatus::Waiting || existing.status == MissionQueueStatus::Active)) {
            addHistory(QStringLiteral("QUEUE_DUPLICATE_IGNORED"), buttonId, displayNameForWaypoint(buttonId), missionId, existing.queueId,
                       QStringLiteral("IGNORED"), QStringLiteral("동일 위치 호출이 이미 대기 또는 진행 중"));
            emit historyChanged();
            return;
        }
    }

    MissionQueueItem item;
    item.queueId = nextQueueId();
    item.waypointId = buttonId;
    item.waypointName = displayNameForWaypoint(buttonId);
    item.requestedAt = requestedAt.isValid() ? requestedAt : QDateTime::currentDateTime();
    item.enqueueOrder = m_queueItems.size() + 1;
    item.status = MissionQueueStatus::Waiting;
    item.requestSource = requestSource.isEmpty() ? QStringLiteral("ros2") : requestSource;
    item.missionId = missionId;

    QString errorMessage;
    if (!m_waypointRepository->waypointPose(buttonId, item.pose, errorMessage)) {
        addHistory(QStringLiteral("QUEUE_FAILED"), buttonId, item.waypointName, missionId, item.queueId,
                   QStringLiteral("FAILED"), errorMessage);
        emit historyChanged();
        return;
    }

    m_queueItems.append(item);
    saveQueue();
    const MissionQueueItem persistedItem = m_queueItems.last();
    addHistory(QStringLiteral("QUEUE_REGISTERED"), persistedItem.waypointId, persistedItem.waypointName,
               persistedItem.missionId, persistedItem.queueId,
               QStringLiteral("WAITING"), QStringLiteral("호출 큐 등록"));
    log(QStringLiteral("QUEUE"), QStringLiteral("registered queue_id=%1 waypoint_id=%2")
                                    .arg(persistedItem.queueId, persistedItem.waypointId));
    emit queueChanged();
    emit historyChanged();
    startNextQueueIfPossible();
}

void MissionController::startNextQueueIfPossible()
{
    if (m_snapshot.state != MissionState::Idle && m_snapshot.state != MissionState::Completed
        && m_snapshot.state != MissionState::Cancelled && m_snapshot.state != MissionState::Failed) {
        return;
    }
    if (m_snapshot.returningHome) {
        return;
    }

    for (const MissionQueueItem &item : std::as_const(m_queueItems)) {
        if (item.status == MissionQueueStatus::Waiting) {
            log(QStringLiteral("QUEUE"), QStringLiteral("dequeue next queue_id=%1").arg(item.queueId));
            startMissionForQueue(item.queueId, false);
            return;
        }
    }

    resetToIdleSnapshot();
}

void MissionController::reconcileOrphanedActiveMission()
{
    for (const MissionQueueItem &item : std::as_const(m_queueItems)) {
        if (item.status != MissionQueueStatus::Active) {
            continue;
        }
        m_snapshot.missionId = item.missionId;
        m_snapshot.queueId = item.queueId;
        m_snapshot.originName = QStringLiteral("이전 세션");
        m_snapshot.destinationName = item.waypointName;
        m_snapshot.currentWaypointName = item.waypointName;
        m_snapshot.nextWaypointName = item.waypointName;
        m_snapshot.startedAt = item.requestedAt.isValid() ? item.requestedAt : QDateTime::currentDateTime();
        m_snapshot.elapsedSeconds = m_snapshot.startedAt.secsTo(QDateTime::currentDateTime());
        m_snapshot.waitingCount = std::count_if(m_queueItems.cbegin(), m_queueItems.cend(), [](const MissionQueueItem &candidate) {
            return candidate.status == MissionQueueStatus::Waiting;
        });
        m_snapshot.returningHome = false;
        m_snapshot.arrivedBeforeEnd = false;
        m_snapshot.payloadRemoved = false;
        m_snapshot.failureReason.clear();
        m_snapshot.distanceRemainingMeters = -1.0;
        // setMissionState()는 Idle -> Navigating 전환을 감지해 missionChanged()를
        // 알아서 emit한다.
        setMissionState(MissionState::Navigating);
        addHistory(QStringLiteral("MISSION_RECOVERED"), item.waypointId, item.waypointName, item.missionId, item.queueId,
                   QStringLiteral("RECOVERED"),
                   QStringLiteral("대시보드가 미션 진행 중 종료됐다가 다시 시작됨 — 정확한 진행 단계를 몰라 "
                                  "\"출동 중\"으로 임시 표시합니다. 실제 로봇 상태를 확인 후 필요하면 미션을 취소해주세요."));
        emit historyChanged();
        return; // 정상 흐름에서 Active 항목은 한 번에 하나뿐이라 첫 건만 처리
    }
}

void MissionController::startMissionForQueue(const QString &queueId, bool divertedFromActiveMission)
{
    const int index = queueIndex(queueId);
    if (index < 0) {
        return;
    }
    // 직전 미션의 완료 체크마크 표시를 위한 30초 대기 중이었더라도, 새 미션이
    // 실제로 시작되면 그 대기는 더 이상 의미가 없다 — 정리해둔다.
    m_completionResetTimer->stop();

    MissionQueueItem &item = m_queueItems[index];
    item.status = MissionQueueStatus::Active;
    item.missionId = item.missionId.isEmpty() ? nextMissionId() : item.missionId;

    m_snapshot.missionId = item.missionId;
    m_snapshot.queueId = item.queueId;
    m_snapshot.originName = m_robotPoseValid ? nearestWaypointName(m_lastRobotX, m_lastRobotY) : QStringLiteral("현재 위치");
    m_snapshot.destinationName = item.waypointName;
    m_snapshot.currentWaypointName = m_snapshot.originName;
    m_snapshot.nextWaypointName = item.waypointName;
    m_snapshot.startedAt = QDateTime::currentDateTime();
    m_snapshot.elapsedSeconds = 0;
    m_snapshot.waitingCount = std::count_if(m_queueItems.cbegin(), m_queueItems.cend(), [](const MissionQueueItem &candidate) {
        return candidate.status == MissionQueueStatus::Waiting;
    });
    m_snapshot.returningHome = false;
    m_snapshot.arrivedBeforeEnd = false;
    m_snapshot.payloadRemoved = false;
    m_snapshot.failureReason.clear();
    m_snapshot.distanceRemainingMeters = -1.0;
    m_initialDistanceMeters = -1.0;
    if (m_robotPoseValid) {
        m_initialDistanceMeters = std::hypot(item.pose.positionX - m_lastRobotX, item.pose.positionY - m_lastRobotY);
    }

    setMissionState(MissionState::Starting);
    addHistory(divertedFromActiveMission ? QStringLiteral("MISSION_DIVERTED_STARTED") : QStringLiteral("MISSION_STARTED"),
               item.waypointId, item.waypointName, item.missionId, item.queueId,
               QStringLiteral("STARTED"), divertedFromActiveMission ? QStringLiteral("확인 전환 후 새 미션 시작")
                                                                     : QStringLiteral("호출 미션 시작"));
    saveQueue();
    item = m_queueItems[index];
    m_snapshot.missionId = item.missionId;
    m_snapshot.queueId = item.queueId;
    emit queueChanged();
    emit historyChanged();
    emit missionChanged();
    log(QStringLiteral("NAV2"),
        QStringLiteral("goal send mission_id=%1 queue_id=%2 waypoint_id=%3").arg(item.missionId, item.queueId, item.waypointId));
    QVector<NavPose> poses;
    // 경로(trajectory) 다지점 주행이 단독 웨이포인트 주행에도 영향을 줘서
    // (2026-08-24) 다시 비활성화 — 항상 단일 지점(navigateToPose)으로 보낸다.
    // if (!trajectoryPosesForButton(item.waypointId, poses)) {
    //     poses = {NavPose{item.pose.positionX, item.pose.positionY, item.pose.yawRadians}};
    // }
    poses = {NavPose{item.pose.positionX, item.pose.positionY, item.pose.yawRadians}};
    dispatchNavigation(poses, item.pose.frameId, item.waypointName);

    // 문서 02 §7.1 음성 안내 #3(SR-F-005) "임무 시작" — 호출을 받아 실제로
    // 주행을 시작하는 순간 1회 안내한다. beginArrivedWait()의 "AED가
    // 도착했습니다"와 동일한 방식(Pi의 robot_audio_output이 처리).
    m_rosInterface->publishAudioAnnouncement(QStringLiteral("응급 호출을 접수했습니다. 출동합니다"));
}

bool MissionController::beginReturnHomeFlow(const QString &reason)
{
    if (m_snapshot.state == MissionState::ReturningHome || m_snapshot.state == MissionState::Parking) {
        return false;
    }

    if (m_snapshot.state == MissionState::Idle) {
        WaypointDef homeWaypoint;
        WaypointPose homePose;
        QString errorMessage;
        if (!resolveHomeWaypoint(homeWaypoint, homePose, errorMessage)) {
            addHistory(QStringLiteral("RETURN_HOME_FAILED"), QString(), QString(), QString(), QString(),
                       QStringLiteral("FAILED"), errorMessage);
            emit historyChanged();
            return false;
        }

        m_snapshot.missionId = nextMissionId();
        m_snapshot.queueId.clear();
        m_snapshot.originName = m_robotPoseValid ? nearestWaypointName(m_lastRobotX, m_lastRobotY) : QStringLiteral("현재 위치");
        m_snapshot.destinationName = homeWaypoint.label;
        m_snapshot.nextWaypointName = homeWaypoint.label;
        m_snapshot.startedAt = QDateTime::currentDateTime();
        m_snapshot.returningHome = true;
        m_snapshot.payloadRemoved = false;
        m_pendingAction = PendingAction::ReturnHome;
        m_pendingReason = reason;
        setMissionState(MissionState::ReturningHome);
        addHistory(QStringLiteral("RETURN_HOME_STARTED"), homeWaypoint.name, homeWaypoint.label, m_snapshot.missionId, QString(),
                   QStringLiteral("STARTED"), reason);
        emit historyChanged();
        emit missionChanged();
        m_rosInterface->publishAudioAnnouncement(QStringLiteral("홈으로 복귀합니다"));
        m_navigationService->navigateToPose(homePose.positionX, homePose.positionY, homePose.yawRadians, homePose.frameId);
        return true;
    }

    if (m_snapshot.state == MissionState::Arrived) {
        // Arrived는 Nav2 goal이 이미 SUCCEEDED로 끝난 뒤라 취소할 활성 goal이
        // 없다 — 아래 else 분기(cancelGoal 후 CANCELED 콜백을 기다림)를 타면
        // CANCELED가 영영 안 와서 멈춘다. Idle 분기처럼 바로 홈 goal을
        // 보내되, 이미 진행 중인 미션이라 missionId/queueId는 새로 만들지
        // 않고 그대로 이어간다. 운영자가 지금 직접 개입하는 거라 PC가 바로
        // 처리하지만, Pi의 arrival_return_manager가 나중에 또 자기 판단으로
        // 복귀 goal을 쏘면 안 되므로 먼저 해제 신호를 보낸다.
        m_rosInterface->publishArrivalNotice(false, m_snapshot.missionId, m_snapshot.queueId, 0.0, 0.0, 0.0, QString());
        WaypointDef homeWaypoint;
        WaypointPose homePose;
        QString errorMessage;
        if (!resolveHomeWaypoint(homeWaypoint, homePose, errorMessage)) {
            finishMission(MissionState::Failed, errorMessage, QStringLiteral("RETURN_HOME_FAILED"), QStringLiteral("FAILED"));
            return false;
        }
        addHistory(QStringLiteral("RETURN_HOME_STARTED"), homeWaypoint.name, homeWaypoint.label, m_snapshot.missionId, m_snapshot.queueId,
                   QStringLiteral("STARTED"), reason);
        m_snapshot.destinationName = homeWaypoint.label;
        m_snapshot.nextWaypointName = homeWaypoint.label;
        m_snapshot.returningHome = true;
        setMissionState(MissionState::ReturningHome);
        emit historyChanged();
        emit missionChanged();
        m_rosInterface->publishAudioAnnouncement(QStringLiteral("홈으로 복귀합니다"));
        dispatchReturnHomeNavigation(homePose, homeWaypoint.label);
        return true;
    }

    if (m_snapshot.state == MissionState::Cancelling || m_snapshot.state == MissionState::Diverting) {
        return false;
    }

    m_pendingAction = PendingAction::ReturnHome;
    m_pendingReason = reason;
    setMissionState(MissionState::Cancelling);
    addHistory(QStringLiteral("RETURN_HOME_REQUESTED"),
               QString(), m_snapshot.destinationName, m_snapshot.missionId, m_snapshot.queueId,
               QStringLiteral("REQUESTED"), reason);
    emit historyChanged();
    m_navigationService->cancelGoal();
    return true;
}

bool MissionController::dispatchPendingReturnHome()
{
    m_pendingAction = PendingAction::None;
    WaypointDef homeWaypoint;
    WaypointPose homePose;
    QString errorMessage;
    if (!resolveHomeWaypoint(homeWaypoint, homePose, errorMessage)) {
        finishMission(MissionState::Failed, errorMessage, QStringLiteral("RETURN_HOME_FAILED"), QStringLiteral("FAILED"));
        return false;
    }
    addHistory(QStringLiteral("RETURN_HOME_STARTED"), homeWaypoint.name, homeWaypoint.label, m_snapshot.missionId, m_snapshot.queueId,
               QStringLiteral("STARTED"), QStringLiteral("홈 복귀 시작"));
    m_snapshot.destinationName = homeWaypoint.label;
    m_snapshot.nextWaypointName = homeWaypoint.label;
    m_snapshot.returningHome = true;
    setMissionState(MissionState::ReturningHome);
    m_rosInterface->publishAudioAnnouncement(QStringLiteral("홈으로 복귀합니다"));
    dispatchReturnHomeNavigation(homePose, homeWaypoint.label);
    return true;
}

void MissionController::beginArrivedWait()
{
    m_snapshot.payloadRemoved = false;
    setMissionState(MissionState::Arrived);
    addHistory(QStringLiteral("MISSION_ARRIVED"), QString(), m_snapshot.destinationName, m_snapshot.missionId, m_snapshot.queueId,
               QStringLiteral("ARRIVED"), QStringLiteral("호출구역 도착 — AED 반출 대기"));
    emit historyChanged();
    emit missionChanged();

    // 문서 02 §7.1 음성 안내 #6. 도착 판단 자체는 Nav2(PC)에서 나오는 거라
    // 이 발행은 여전히 그 순간의 Wi-Fi에 걸려있다 — 다만 이후 반출 감지/
    // 타이머/복귀 goal 전송은 전부 Pi(arrival_return_manager)가 로컬로
    // 이어받는다(2026-08-19).
    m_rosInterface->publishAudioAnnouncement(QStringLiteral("AED가 도착했습니다"));

    WaypointDef homeWaypoint;
    WaypointPose homePose;
    QString errorMessage;
    if (!resolveHomeWaypoint(homeWaypoint, homePose, errorMessage)) {
        // 반출 감시 자체를 못 켠다 — 자동 복귀는 안 되지만 미션을 실패
        // 처리하진 않는다(운영자가 홈을 나중에 설정하고 수동 "복귀" 버튼으로
        // 여전히 끝낼 수 있음). 기록만 남긴다.
        addHistory(QStringLiteral("ARRIVAL_WATCH_NOT_ARMED"), QString(), m_snapshot.destinationName, m_snapshot.missionId,
                   m_snapshot.queueId, QStringLiteral("FAILED"),
                   QStringLiteral("자동 복귀 감시를 시작하지 못했습니다: %1").arg(errorMessage));
        emit historyChanged();
        return;
    }

    // 이 미션을 유발한 버튼의 trajectory도 이 시점(Wi-Fi가 아직 살아있을 때)에
    // 통째로 같이 실어 보내는 기능이 있었으나, Pi 쪽 navigate_through_poses
    // 구현(arrival_return_manager.py)은 PC의 followWaypoints와 달리 approach
    // pose가 없어 코너에서 제자리 회전을 일으킬 수 있다 — Pi 쪽에도 같은
    // 보완이 되기 전까지는 의도적으로 비활성 상태로 둔다(2026-08-24). 항상
    // 빈 목록을 보내 Pi는 HOME 단일 지점으로만 복귀한다.
    QVector<NavPose> trajectoryPoses;
    // if (const MissionQueueItem *item = activeQueueItem()) {
    //     trajectoryPosesForButton(item->waypointId, trajectoryPoses);
    // }
    m_rosInterface->publishArrivalNotice(true, m_snapshot.missionId, m_snapshot.queueId, homePose.positionX,
                                         homePose.positionY, homePose.yawRadians, homePose.frameId, trajectoryPoses);

    // Pi가 실제로 타게 될 전체 경로(trajectory+HOME)를 캐싱해둔다 — "returning"
    // 상태를 받았을 때 DB에 다시 조회하지 않고 그대로 robot_session/nav_goal_log
    // 기록에 쓴다(handlePiReturnStatus 참고).
    m_pendingPiReturnPoses = trajectoryPoses;
    m_pendingPiReturnPoses.append(NavPose{homePose.positionX, homePose.positionY, homePose.yawRadians});
}

void MissionController::handleDockResult(bool success, const QString &detail)
{
    // 정상적으로는 항상 Parking 상태에서 온다. 다만 도착~복귀 전 구간이 이제
    // Pi(arrival_return_manager)가 로컬로 처리하는 부분이라, Wi-Fi가 그동안
    // 끊겨있었으면 PC는 Arrived나 ReturningHome에 멈춰있는 채로 이 결과를
    // 처음 받을 수 있다 — 그런 경우도 놓치지 않고 미션을 마무리하도록
    // 범위를 넓힌다(2026-08-19).
    if (m_snapshot.state != MissionState::Parking && m_snapshot.state != MissionState::ReturningHome
        && m_snapshot.state != MissionState::Arrived) {
        return;
    }
    if (success) {
        m_rosInterface->publishAudioAnnouncement(QStringLiteral("도킹 완료했습니다"));
        finishMission(MissionState::Completed, m_pendingReason, QStringLiteral("PARKING_COMPLETED"), QStringLiteral("SUCCEEDED"));
        return;
    }
    if (m_pendingReason.contains(QStringLiteral("취소"))) {
        finishMission(MissionState::Cancelled, m_pendingReason, QStringLiteral("PARKING_CANCELLED"), QStringLiteral("CANCELLED"));
        return;
    }
    const QString reason = detail.isEmpty() ? QStringLiteral("ArUco 전면주차 실패") : QStringLiteral("ArUco 전면주차 실패: %1").arg(detail);
    finishMission(MissionState::Failed, reason, QStringLiteral("PARKING_FAILED"), QStringLiteral("FAILED"));
}

void MissionController::handlePiReturnStatus(const QString &phase, const QString &missionId, const QString &queueId,
                                             int waypointIndex, int posesTotal, bool replayed)
{
    Q_UNUSED(posesTotal)
    // Pi가 지금 진행 중인 우리 미션 얘기를 하는 게 맞는지 확인 — 다른(예:
    // 이미 끝난) 미션의 지연 도착 메시지에 반응하지 않게.
    if (missionId != m_snapshot.missionId || queueId != m_snapshot.queueId) {
        return;
    }
    if (replayed) {
        // Wi-Fi 단절 중 Pi 로컬에 버퍼링돼 있다가 재연결 후 뒤늦게 온 이벤트 —
        // 처리 로직은 아래와 동일(상태 가드로 재실행에 안전), 로그만 표시.
        log(QStringLiteral("MISSION"), QStringLiteral("Pi 재전송 이벤트 수신 phase=%1").arg(phase));
    }

    if (phase == QStringLiteral("payload_removed")) {
        if (!m_snapshot.payloadRemoved) {
            m_snapshot.payloadRemoved = true;
            addHistory(QStringLiteral("PAYLOAD_REMOVED_DETECTED"), QString(), m_snapshot.destinationName, missionId, queueId,
                       QStringLiteral("DETECTED"), QStringLiteral("AED 반출 감지(Pi) — 잠시 후 자동 복귀"));
            emit historyChanged();
            emit missionChanged();
        }
        return;
    }

    if (phase == QStringLiteral("returning")) {
        // Pi가 이미 Nav2에 직접 복귀 goal을 보낸 뒤다 — 여기서 또
        // navigateToPose를 부르면 안 된다(중복 goal). 화면 상태만 맞춘다.
        if (m_snapshot.state == MissionState::Arrived) {
            addHistory(QStringLiteral("RETURN_HOME_STARTED"), QString(), m_snapshot.destinationName, missionId, queueId,
                       QStringLiteral("STARTED"), QStringLiteral("Pi 자동 복귀 시작"));
            m_snapshot.returningHome = true;
            setMissionState(MissionState::ReturningHome);
            emit historyChanged();
            emit missionChanged();

            // Pi가 실제로 주행을 시작한 시점 — NavGoalLogRecorder는 로컬
            // NavigationService 시그널만 보므로 이 Pi 주도 복귀는 여기서 직접
            // robot_session/nav_goal_log를 연다. beginArrivedWait()가 arrival_notice와
            // 함께 캐싱해둔 경로(m_pendingPiReturnPoses)를 그대로 쓴다.
            if (m_robotService && m_overviewService && !m_pendingPiReturnPoses.isEmpty()) {
                const QVector<RobotRecord> robots = m_robotService->listRobots(false);
                if (robots.size() == 1) {
                    QVector<NavGoalPoint> goals;
                    goals.reserve(m_pendingPiReturnPoses.size());
                    for (const NavPose &pose : m_pendingPiReturnPoses) {
                        goals.append(NavGoalPoint{pose.x, pose.y, pose.yaw});
                    }
                    const NavGoalPoint startPose{m_lastRobotX, m_lastRobotY, 0.0};
                    QString dbErrorMessage;
                    if (!m_overviewService->startRobotSession(robots.first().id, QStringLiteral("auto"),
                                                              QStringLiteral("Pi 로컬 복귀"), goals, m_robotPoseValid,
                                                              startPose, m_piReturnSessionId, dbErrorMessage)) {
                        log(QStringLiteral("MISSION"), QStringLiteral("Pi 복귀 세션 기록 실패: %1").arg(dbErrorMessage));
                        m_piReturnSessionId = -1;
                    }
                    m_piReturnLastMarkedIndex = -1;
                }
            }
        }

        // 매번(첫 진입 포함) 넘어온 진행률로, 이번에 새로 지나친 지점까지
        // 'arrived'로 마감한다. 첫 "returning"은 아직 goal도 안 나간 시점이라
        // waypointIndex가 -1(진행률 없음)로 온다 — 그때는 아무것도 안 함.
        if (m_piReturnSessionId >= 0 && waypointIndex > m_piReturnLastMarkedIndex) {
            QString dbErrorMessage;
            for (int i = m_piReturnLastMarkedIndex + 1; i < waypointIndex; ++i) {
                m_overviewService->markGoalReached(m_piReturnSessionId, i, dbErrorMessage);
            }
            m_piReturnLastMarkedIndex = waypointIndex - 1;
        }
        return;
    }

    if (phase == QStringLiteral("return_failed")) {
        if (m_snapshot.state == MissionState::Arrived || m_snapshot.state == MissionState::ReturningHome) {
            finishMission(MissionState::Failed, QStringLiteral("Pi 자동 복귀 실패"), QStringLiteral("RETURN_HOME_FAILED"),
                          QStringLiteral("FAILED"));
        }
        return;
    }
    // "armed"는 화면에 딱히 반영할 게 없어 로그만 남고 지나간다.
}

bool MissionController::trajectoryPosesForButton(const QString &buttonCode, QVector<NavPose> &poses) const
{
    if (!m_robotService || !m_overviewService || buttonCode.isEmpty()) {
        return false;
    }
    // trajectory는 waypoint_set(robot_id, type='trajectory')에 저장돼 로봇마다
    // 갈리므로, 운영현황 자동기록과 동일하게 로봇이 정확히 1대 등록돼 있을 때만
    // 조회한다(waypointmanagementpage.cpp::handleSaveTrajectory와 동일 패턴).
    const QVector<RobotRecord> robots = m_robotService->listRobots(false);
    if (robots.size() != 1) {
        return false;
    }

    QString trajectoryName;
    QVector<NavGoalPoint> dbPoses;
    QString errorMessage;
    if (!m_overviewService->loadTrajectoryPosesForButton(robots.first().id, buttonCode, trajectoryName, dbPoses, errorMessage)) {
        log(QStringLiteral("MISSION"), QStringLiteral("경로 조회 실패(버튼=%1): %2").arg(buttonCode, errorMessage));
        return false;
    }
    if (dbPoses.isEmpty()) {
        return false;
    }

    poses.clear();
    poses.reserve(dbPoses.size());
    for (const NavGoalPoint &point : dbPoses) {
        poses.append(NavPose{point.x, point.y, point.yaw});
    }
    log(QStringLiteral("MISSION"),
        QStringLiteral("버튼 %1 → 경로 '%2'(%3점) 사용").arg(buttonCode, trajectoryName).arg(poses.size()));
    return true;
}

void MissionController::dispatchNavigation(const QVector<NavPose> &poses, const QString &frameId, const QString &label)
{
    if (poses.isEmpty()) {
        return;
    }
    if (poses.size() == 1) {
        m_navigationService->navigateToPose(poses.first().x, poses.first().y, poses.first().yaw, frameId, label);
    } else {
        // navigateThroughPoses는 approach pose를 끼워넣어도(2026-08-24 수정) 또
        // 다른 코너에서 제자리 회전이 재현되어 일단 비활성화한다. followWaypoints는
        // 각 지점마다 잠깐 정지하지만 approach pose 포함 검증된 경로라 이걸로 대체.
        // m_navigationService->navigateThroughPoses(poses, frameId, label);
        m_navigationService->followWaypoints(poses, frameId, label);
    }
}

void MissionController::dispatchReturnHomeNavigation(const WaypointPose &homePose, const QString &label)
{
    QVector<NavPose> poses;
    // 출동과 동일한 이유(2026-08-24)로 trajectory 조회를 다시 비활성화 —
    // 항상 HOME 단일 지점으로만 복귀한다.
    // if (const MissionQueueItem *item = activeQueueItem()) {
    //     trajectoryPosesForButton(item->waypointId, poses);
    // }
    poses.append(NavPose{homePose.positionX, homePose.positionY, homePose.yawRadians});
    dispatchNavigation(poses, homePose.frameId, label);
}

bool MissionController::resolveHomeWaypoint(WaypointDef &homeWaypoint, WaypointPose &homePose, QString &errorMessage) const
{
    // Home 도착 후 항상 ArUco 전면주차로 이어진다(handleNavigationResult
    // 참고). simulate_docking(기본 true)이면 카메라 없는 로봇도 안전하게
    // 그대로 완료되므로 Home 웨이포인트 하나만 있으면 된다.
    const QVector<WaypointDef> homes = m_waypointRepository->waypointsByKind(WaypointMarkerKind::Home);
    if (homes.isEmpty()) {
        errorMessage = QStringLiteral("홈 waypoint가 설정되어 있지 않습니다.");
        log(QStringLiteral("MISSION"), QStringLiteral("home waypoint lookup failed: missing"));
        return false;
    }
    if (homes.size() > 1) {
        errorMessage = QStringLiteral("홈 waypoint가 여러 개입니다. 대표 홈 1개만 남겨야 합니다.");
        log(QStringLiteral("MISSION"), QStringLiteral("home waypoint lookup failed: duplicated"));
        return false;
    }

    homeWaypoint = homes.first();
    const bool ok = m_waypointRepository->waypointPose(homeWaypoint.name, homePose, errorMessage);
    if (ok) {
        log(QStringLiteral("MISSION"), QStringLiteral("returning home waypoint_id=%1").arg(homeWaypoint.name));
    }
    return ok;
}

void MissionController::finishMission(MissionState finalState,
                                      const QString &reason,
                                      const QString &historyType,
                                      const QString &historyResult,
                                      bool requiresQueueConfirmation)
{
    if (MissionQueueItem *item = activeQueueItem()) {
        item->status = (finalState == MissionState::Completed)
                           ? MissionQueueStatus::Completed
                           : (finalState == MissionState::Cancelled ? MissionQueueStatus::Cancelled : MissionQueueStatus::Failed);
    }
    addHistory(historyType, QString(), m_snapshot.destinationName, m_snapshot.missionId, m_snapshot.queueId, historyResult,
               reason.isEmpty() ? resultText(historyResult) : reason);
    m_snapshot.failureReason = reason;
    // returningHome은 바로 아래서 false로 리셋해야 다음 대기 호출이 안 막히니,
    // 실시간 관제 스텝퍼가 "복귀 단계까지는 갔었는지" 나중에 구분할 수 있게
    // 그 순간의 값을 arrivedBeforeEnd에 남겨둔다. Arrived(도착 후 반출 대기
    // 중 resolveHomeWaypoint 실패 등으로 바로 실패 처리되는 경우)도 "도착은
    // 했었다"이므로 포함한다.
    m_snapshot.arrivedBeforeEnd = m_snapshot.returningHome || m_snapshot.state == MissionState::Arrived
        || finalState == MissionState::Completed;
    // 미션이 어떤 이유로든 끝나는 시점 — Pi(arrival_return_manager)가 이
    // 미션에 대해 여전히 armed 상태로 남아 나중에 또 복귀 goal을 쏘는 일이
    // 없도록 해제 신호를 같이 보낸다(이미 해제돼 있으면 Pi가 그냥 무시함).
    if (!m_snapshot.missionId.isEmpty()) {
        m_rosInterface->publishArrivalNotice(false, m_snapshot.missionId, m_snapshot.queueId, 0.0, 0.0, 0.0, QString());
    }
    // Pi 주도 복귀(handlePiReturnStatus)로 DB 세션을 열어뒀으면, 이 미션이
    // 어떤 이유로 끝나든 그 세션도 같이 마감한다 — NavGoalLogRecorder가 이
    // 세션의 존재를 모르므로 여기서 직접 닫아야 한다.
    if (m_overviewService && m_piReturnSessionId >= 0) {
        QString dbErrorMessage;
        m_overviewService->finishRobotSession(m_piReturnSessionId, finalState == MissionState::Completed,
                                              finalState == MissionState::Cancelled, dbErrorMessage);
        m_piReturnSessionId = -1;
        m_piReturnLastMarkedIndex = -1;
    }
    setMissionState(finalState);
    m_pendingAction = PendingAction::None;
    m_pendingDivertQueueId.clear();
    m_snapshot.returningHome = false;
    saveQueue();
    emit queueChanged();
    emit historyChanged();
    emit missionChanged();

    if (requiresQueueConfirmation) {
        for (const MissionQueueItem &item : std::as_const(m_queueItems)) {
            if (item.status == MissionQueueStatus::Waiting) {
                log(QStringLiteral("QUEUE"), QStringLiteral("awaiting operator confirmation queue_id=%1").arg(item.queueId));
                resetToIdleSnapshot();
                emit nextQueueConfirmationRequested(item.queueId, item.waypointName);
                return;
            }
        }
    }

    if (finalState == MissionState::Completed) {
        // 완료 체크마크를 30초간 화면에 보여준 뒤에야 다음 큐 처리/Idle
        // 초기화로 넘어간다. 그 사이 새 호출이 들어오면 enqueueCall()이
        // 직접 startNextQueueIfPossible()을 불러 즉시 처리하므로 대기가
        // 필요 없다 — 그때는 이 타이머가 나중에 만료돼도 이미 Idle/Completed가
        // 아니라 startNextQueueIfPossible()의 상태 가드에서 아무 일도 안 한다.
        m_completionResetTimer->start(30000);
    } else {
        startNextQueueIfPossible();
    }
}

void MissionController::resetToIdleSnapshot()
{
    m_snapshot = MissionSnapshot();
    m_snapshot.waitingCount = std::count_if(m_queueItems.cbegin(), m_queueItems.cend(), [](const MissionQueueItem &item) {
        return item.status == MissionQueueStatus::Waiting;
    });
    emit missionChanged();
}

void MissionController::confirmStartNextQueue(const QString &queueId)
{
    if (m_snapshot.state != MissionState::Idle) {
        return;
    }
    const int index = queueIndex(queueId);
    if (index < 0 || m_queueItems.at(index).status != MissionQueueStatus::Waiting) {
        return;
    }
    log(QStringLiteral("QUEUE"), QStringLiteral("operator confirmed next queue_id=%1").arg(queueId));
    startMissionForQueue(queueId, false);
}

void MissionController::declineNextQueue()
{
    log(QStringLiteral("QUEUE"), QStringLiteral("operator declined next queue start, staying stopped"));
    addHistory(QStringLiteral("QUEUE_START_DECLINED"), QString(), QString(), QString(), QString(),
               QStringLiteral("DECLINED"), QStringLiteral("다음 호출 주행을 보류하고 현재 위치에서 대기"));
    emit historyChanged();
}

void MissionController::markQueueStatus(const QString &queueId, MissionQueueStatus status)
{
    const int index = queueIndex(queueId);
    if (index >= 0) {
        m_queueItems[index].status = status;
    }
}

int MissionController::queueIndex(const QString &queueId) const
{
    for (int i = 0; i < m_queueItems.size(); ++i) {
        if (m_queueItems.at(i).queueId == queueId) {
            return i;
        }
    }
    return -1;
}

MissionQueueItem *MissionController::activeQueueItem()
{
    const int index = queueIndex(m_snapshot.queueId);
    if (index < 0) {
        return nullptr;
    }
    return &m_queueItems[index];
}

const MissionQueueItem *MissionController::activeQueueItem() const
{
    const int index = queueIndex(m_snapshot.queueId);
    if (index < 0) {
        return nullptr;
    }
    return &m_queueItems[index];
}

void MissionController::setMissionState(MissionState state)
{
    if (m_snapshot.state == state) {
        return;
    }
    log(QStringLiteral("MISSION"),
        QStringLiteral("state: %1 -> %2").arg(missionStateText(m_snapshot.state), missionStateText(state)));
    m_snapshot.state = state;
    emit missionChanged();
}

void MissionController::saveQueue()
{
    m_storageService->saveQueue(m_queueItems);
    if (MissionQueueItem *item = activeQueueItem()) {
        m_snapshot.missionId = item->missionId;
        m_snapshot.queueId = item->queueId;
    }
}

void MissionController::addHistory(const QString &eventType,
                                   const QString &waypointId,
                                   const QString &waypointName,
                                   const QString &missionId,
                                   const QString &queueId,
                                   const QString &result,
                                   const QString &detailMessage)
{
    MissionHistoryEntry entry;
    entry.historyId = historyId();
    entry.occurredAt = QDateTime::currentDateTime();
    entry.eventType = eventType;
    entry.waypointId = waypointId;
    entry.waypointName = waypointName;
    entry.missionId = missionId;
    entry.queueId = queueId;
    entry.result = result;
    entry.detailMessage = detailMessage;
    m_storageService->appendHistory(entry);
    m_historyEntries.prepend(entry);
    while (m_historyEntries.size() > 100) {
        m_historyEntries.removeLast();
    }
    log(QStringLiteral("HISTORY"), QStringLiteral("event saved type=%1").arg(eventType));
}

QString MissionController::nextMissionId() const
{
    return QStringLiteral("MR-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddhhmmsszzz")));
}

QString MissionController::nextQueueId() const
{
    return QStringLiteral("Q-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

QString MissionController::displayNameForWaypoint(const QString &waypointId) const
{
    WaypointDef waypoint;
    if (m_waypointRepository->waypoint(waypointId, waypoint)) {
        return waypoint.label.isEmpty() ? waypoint.name : waypoint.label;
    }
    return waypointId;
}

QString MissionController::nearestWaypointName(double x, double y) const
{
    const WaypointMap waypoints = m_waypointRepository->waypoints();
    double nearestDistance = std::numeric_limits<double>::max();
    QString nearestName = QStringLiteral("현재 위치");
    for (auto it = waypoints.cbegin(); it != waypoints.cend(); ++it) {
        const double distance = std::hypot(it.value().x - x, it.value().y - y);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestName = it.value().label.isEmpty() ? it.key() : it.value().label;
        }
    }
    return nearestName;
}

void MissionController::updateProgress()
{
    if (m_initialDistanceMeters <= 0.0 || m_snapshot.distanceRemainingMeters < 0.0) {
        m_snapshot.progressPercent = -1;
        return;
    }
    const double ratio = 1.0 - (m_snapshot.distanceRemainingMeters / m_initialDistanceMeters);
    m_snapshot.progressPercent = qBound(0, static_cast<int>(ratio * 100.0), 100);
}

void MissionController::updateConnectionState()
{
    m_rosConnected = m_lastRosHeartbeat.isValid()
        && m_lastRosHeartbeat.secsTo(QDateTime::currentDateTime()) <= 5;
}

void MissionController::log(const QString &category, const QString &message) const
{
    qInfo().noquote() << QStringLiteral("[%1] %2").arg(category, message);
}
