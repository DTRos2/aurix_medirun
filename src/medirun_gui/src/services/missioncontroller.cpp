#include "missioncontroller.h"

#include "../ros/navigationservice.h"
#include "missionrosinterface.h"
#include "missionstorageservice.h"
#include "waypointrepository.h"

#include <QDateTime>
#include <QDebug>
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
                                     QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_navigationService(navigationService)
    , m_waypointRepository(waypointRepository)
    , m_storageService(storageService)
    , m_rosInterface(rosInterface)
{
    m_storageService->setSessionContext(m_session);
    m_queueItems = m_storageService->loadQueue();
    m_historyEntries = m_storageService->loadTodayHistory();
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &MissionController::updateElapsed);
    m_elapsedTimer->start();

    connect(m_waypointRepository, &WaypointRepository::repositoryChanged, this, &MissionController::handleWaypointRepositoryChanged);

    connect(m_navigationService, &NavigationService::navigationGoalAccepted, this, &MissionController::handleNavigationAccepted);
    connect(m_navigationService, &NavigationService::navigationGoalRejected, this, &MissionController::handleNavigationRejected);
    connect(m_navigationService, &NavigationService::navigationResult, this, &MissionController::handleNavigationResult);
    connect(m_navigationService, &NavigationService::nav2ConnectionChanged, this, &MissionController::handleNav2ConnectionChanged);
    connect(m_navigationService, &NavigationService::feedbackDistanceRemaining, this, &MissionController::handleNavigationFeedback);
    connect(m_navigationService, &NavigationService::amclPoseUpdated, this, &MissionController::handleAmclPoseUpdated);

    connect(m_rosInterface, &MissionRosInterface::dispatchRequested, this, &MissionController::handleDispatchRequested);
    connect(m_rosInterface, &MissionRosInterface::missionHeartbeat, this, &MissionController::handleRosHeartbeat);
    connect(m_rosInterface, &MissionRosInterface::communicationRetryFinished, this, &MissionController::handleRosRetryFinished);

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

    MissionQueueItem item = m_queueItems.at(index);
    m_queueItems.removeAt(index);
    addHistory(QStringLiteral("QUEUE_CANCELLED"), item.waypointId, item.waypointName, item.missionId, item.queueId,
               QStringLiteral("CANCELLED"), QStringLiteral("대기 호출 큐 취소"));
    log(QStringLiteral("QUEUE"), QStringLiteral("cancelled queue_id=%1 waypoint_id=%2").arg(item.queueId, item.waypointId));
    saveQueue();
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
    if (m_snapshot.returningHome || m_snapshot.state == MissionState::Cancelling || m_snapshot.state == MissionState::Diverting) {
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
            WaypointDef homeWaypoint;
            WaypointPose homePose;
            QString errorMessage;
            if (!resolveHomeWaypoint(homeWaypoint, homePose, errorMessage)) {
                finishMission(MissionState::Failed, errorMessage, QStringLiteral("RETURN_HOME_FAILED"), QStringLiteral("FAILED"));
                return;
            }
            addHistory(QStringLiteral("RETURN_HOME_STARTED"), homeWaypoint.name, homeWaypoint.label, m_snapshot.missionId, m_snapshot.queueId,
                       QStringLiteral("STARTED"), QStringLiteral("홈 복귀 시작"));
            m_snapshot.destinationName = homeWaypoint.label;
            m_snapshot.nextWaypointName = homeWaypoint.label;
            m_snapshot.returningHome = true;
            setMissionState(MissionState::ReturningHome);
            m_navigationService->navigateToPose(homePose.positionX, homePose.positionY, homePose.yawRadians, homePose.frameId);
            return;
        }

        // 미션 취소(그 자리에서 정지) — Nav2가 취소를 확인해준 시점. 대기하던
        // 진행 중 호출을 큐에서 완전히 삭제하고 Cancelled로 종료 처리한다.
        // 다음 대기 호출로 바로 주행하지 않고, 운영자에게 확인을 받는다.
        removeQueueItem(m_snapshot.queueId);
        finishMission(MissionState::Cancelled, m_pendingReason.isEmpty() ? QStringLiteral("운영자 미션 취소 요청") : m_pendingReason,
                      QStringLiteral("MISSION_CANCELLED"), QStringLiteral("CANCELLED"), true);
        return;
    }

    if (resultCode == QStringLiteral("SUCCEEDED")) {
        if (m_snapshot.state == MissionState::ReturningHome) {
            finishMission(m_pendingReason.contains(QStringLiteral("취소")) ? MissionState::Cancelled : MissionState::Completed,
                          m_pendingReason, QStringLiteral("RETURN_HOME_COMPLETED"), QStringLiteral("SUCCEEDED"));
            return;
        }
        finishMission(MissionState::Completed, detail, QStringLiteral("MISSION_COMPLETED"), QStringLiteral("SUCCEEDED"));
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

void MissionController::startMissionForQueue(const QString &queueId, bool divertedFromActiveMission)
{
    const int index = queueIndex(queueId);
    if (index < 0) {
        return;
    }

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
    m_navigationService->navigateToPose(item.pose.positionX, item.pose.positionY, item.pose.yawRadians, item.pose.frameId);
}

bool MissionController::beginReturnHomeFlow(const QString &reason)
{
    if (m_snapshot.state == MissionState::ReturningHome) {
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
        m_pendingAction = PendingAction::ReturnHome;
        m_pendingReason = reason;
        setMissionState(MissionState::ReturningHome);
        addHistory(QStringLiteral("RETURN_HOME_STARTED"), homeWaypoint.name, homeWaypoint.label, m_snapshot.missionId, QString(),
                   QStringLiteral("STARTED"), reason);
        emit historyChanged();
        emit missionChanged();
        m_navigationService->navigateToPose(homePose.positionX, homePose.positionY, homePose.yawRadians, homePose.frameId);
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

bool MissionController::resolveHomeWaypoint(WaypointDef &homeWaypoint, WaypointPose &homePose, QString &errorMessage) const
{
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

    startNextQueueIfPossible();
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

void MissionController::removeQueueItem(const QString &queueId)
{
    const int index = queueIndex(queueId);
    if (index >= 0) {
        m_queueItems.removeAt(index);
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
