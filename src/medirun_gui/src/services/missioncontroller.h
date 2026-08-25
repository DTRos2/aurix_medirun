#ifndef MISSIONCONTROLLER_H
#define MISSIONCONTROLLER_H

#include "../app/appsession.h"
#include "../models/missiontypes.h"
#include "../ros/navigationservice.h"

#include <QObject>
#include <QStringList>
#include <QTimer>

class NavigationService;
class WaypointRepository;
class MissionStorageService;
class MissionRosInterface;
class RobotService;
class OperationsOverviewService;

class MissionController : public QObject
{
    Q_OBJECT

public:
    explicit MissionController(const AppSession &session,
                               NavigationService *navigationService,
                               WaypointRepository *waypointRepository,
                               MissionStorageService *storageService,
                               MissionRosInterface *rosInterface,
                               RobotService *robotService,
                               OperationsOverviewService *overviewService,
                               QObject *parent = nullptr);

    MissionSnapshot missionSnapshot() const;
    QVector<MissionQueueItem> waitingQueue() const;
    QVector<MissionHistoryEntry> todayHistory() const;
    QString queueSummaryText() const;
    QString anomalyTitle() const;
    QString anomalyDetail() const;
    QString anomalyMeta() const;
    bool rosConnected() const;
    bool nav2Connected() const;
    bool retryInProgress() const;
    // MQTT 콜존 버튼이 이번 앱 세션 동안 실제로 보낸 raw button_id 목록(최근
    // 감지 순, 등록 성공/실패 여부와 무관 — 웨이포인트관리 페이지가 콜존
    // 등록 시 "감지된 콜존 ID" 드롭다운을 채우는 데 쓴다). 앱 재시작 시 초기화.
    QStringList observedButtonIds() const;

public slots:
    // 웨이포인트 관리 페이지의 수동 "주행 시작"(테스트 주행)은 미션
    // 큐/startMissionForQueue()를 안 거쳐서 원래 TTS 안내가 안 나갔다 —
    // 그 버튼에서 호출한다.
    void announceTestDrive(const QString &waypointLabel);
    void requestRetry();
    void requestReturnHome();
    void requestMissionCancel();
    void requestQueueCancel(const QString &queueId);
    void requestQueueDivert(const QString &queueId);
    void confirmStartNextQueue(const QString &queueId);
    void declineNextQueue();

signals:
    void missionChanged();
    void queueChanged();
    void historyChanged();
    void connectionStateChanged();
    void retryStateChanged();
    void nextQueueConfirmationRequested(const QString &queueId, const QString &waypointName);
    void observedButtonIdsChanged();

private slots:
    void handleDispatchRequested(const QString &buttonId,
                                 const QString &requestSource,
                                 const QString &missionId,
                                 const QDateTime &requestedAt);
    void handleWaypointRepositoryChanged();
    void handleNavigationAccepted();
    void handleNavigationRejected(const QString &reason);
    void handleNavigationResult(const QString &resultCode, const QString &detail);
    void handleNav2ConnectionChanged(bool connected);
    void handleNavigationFeedback(double metersRemaining);
    void handleAmclPoseUpdated(double x, double y, double yawRad);
    void handleRosHeartbeat(const QString &source, const QDateTime &receivedAt);
    void handleRosRetryFinished(bool success, const QString &detail);
    void handleDockResult(bool success, const QString &detail);
    // Pi의 arrival_return_manager가 방송하는 반출감지/복귀 진행상황 — 화면
    // 상태만 따라가고 여기서 Nav2 goal을 다시 보내지는 않는다(그건 Pi가
    // 로컬로 이미 처리함). armed/payload_removed/returning/return_failed.
    void handlePiReturnStatus(const QString &phase, const QString &missionId, const QString &queueId,
                              int waypointIndex, int posesTotal, bool replayed);
    void updateElapsed();

private:
    enum class PendingAction {
        None,
        ReturnHome,
        Divert
    };

    void enqueueCall(const QString &buttonId,
                     const QString &requestSource,
                     const QString &missionId,
                     const QDateTime &requestedAt);
    void startNextQueueIfPossible();
    // 대시보드가 미션 진행 중에 종료됐다가 다시 뜨면, m_snapshot(진행 상태)은
    // 메모리에만 있어 사라지지만 DB 큐에는 그 항목이 MissionQueueStatus::Active로
    // 그대로 남아 있다 — startNextQueueIfPossible()은 Waiting만 보고 Active는
    // 건드리지 않아서, 재시작 후엔 그 미션이 "진행 중인 미션이 없음"으로
    // 보여 취소조차 못 하는 문제가 있었다. 생성자에서 한 번 호출해 그런
    // Active 항목을 찾으면 m_snapshot에 최소한으로 복구해 취소는 가능하게
    // 한다(정확한 진행 단계·Nav2 goal 자체는 이 세션이 모르므로 재현 못 함).
    void reconcileOrphanedActiveMission();
    void startMissionForQueue(const QString &queueId, bool divertedFromActiveMission);
    bool beginReturnHomeFlow(const QString &reason);
    // beginReturnHomeFlow가 cancelGoal()로 기존 목표를 취소하고 CANCELED를
    // 기다리는 중일 때, 취소 확인보다 기존 목표의 SUCCEEDED가 먼저 도착하는
    // 경쟁 상태가 있었다(handleNavigationResult 참고) — CANCELED/SUCCEEDED
    // 어느 쪽으로 오든 대기 중이던 홈 복귀 요청을 동일하게 처리하려고 분리.
    bool dispatchPendingReturnHome();
    // Nav2 goal SUCCEEDED(호출구역 도착) 직후 호출 — Arrived로 전환하고
    // Pi의 arrival_return_manager에 감시 시작(armed=true, 홈 좌표 포함)을
    // 알린다. 반출 감지·타이머·복귀 goal 전송은 그 뒤로 전부 Pi가 로컬로
    // 처리한다(2026-08-19, Wi-Fi 끊김에도 이어가도록 이전).
    void beginArrivedWait();
    // Home 웨이포인트를 홈 복귀 목표로 돌려준다. 도착 후 항상 ArUco
    // 전면주차(Parking)로 이어간다.
    bool resolveHomeWaypoint(WaypointDef &homeWaypoint, WaypointPose &homePose, QString &errorMessage) const;
    // buttonCode에 연결된 trajectory가 DB에 있으면 그 순서 그대로 poses를 채워
    // true를 돌려준다(단, 로봇이 정확히 1대 등록돼 있어야 함 — 그 외엔 폴백).
    // 없거나 조회 실패하면 poses를 안 건드리고 false — 호출자는 이때 기존 단일
    // pose로 주행해야 한다.
    bool trajectoryPosesForButton(const QString &buttonCode, QVector<NavPose> &poses) const;
    // poses.size()==1이면 navigateToPose, 그 이상이면 navigateThroughPoses로
    // 자동 분기하는 출동/복귀 주행 공통 진입점.
    void dispatchNavigation(const QVector<NavPose> &poses, const QString &frameId, const QString &label);
    // 복귀 시 이 미션을 유발한 버튼의 trajectory(저장된 순서 그대로) 뒤에
    // HOME을 이어붙여 한 번에 navigateThroughPoses로 보낸다. activeQueueItem()로
    // 트리거 버튼을 못 찾거나 연결된 trajectory가 없으면 HOME 단일 지점으로
    // 폴백한다(기존 동작과 동일).
    void dispatchReturnHomeNavigation(const WaypointPose &homePose, const QString &label);
    void finishMission(MissionState finalState, const QString &reason, const QString &historyType, const QString &historyResult,
                       bool requiresQueueConfirmation = false);
    void resetToIdleSnapshot();
    void markQueueStatus(const QString &queueId, MissionQueueStatus status);
    int queueIndex(const QString &queueId) const;
    MissionQueueItem *activeQueueItem();
    const MissionQueueItem *activeQueueItem() const;
    void setMissionState(MissionState state);
    void saveQueue();
    void addHistory(const QString &eventType,
                    const QString &waypointId,
                    const QString &waypointName,
                    const QString &missionId,
                    const QString &queueId,
                    const QString &result,
                    const QString &detailMessage);
    QString nextMissionId() const;
    QString nextQueueId() const;
    QString displayNameForWaypoint(const QString &waypointId) const;
    QString nearestWaypointName(double x, double y) const;
    void updateProgress();
    void updateConnectionState();
    void log(const QString &category, const QString &message) const;

    AppSession m_session;
    NavigationService *m_navigationService {nullptr};
    WaypointRepository *m_waypointRepository {nullptr};
    MissionStorageService *m_storageService {nullptr};
    MissionRosInterface *m_rosInterface {nullptr};
    RobotService *m_robotService {nullptr};
    OperationsOverviewService *m_overviewService {nullptr};
    // beginArrivedWait()에서 arrival_notice와 함께 Pi에 보낸 것과 동일한
    // trajectory+HOME 목록 — Pi가 나중에 "returning" 상태를 보고할 때 이걸
    // 그대로 robot_session/nav_goal_log 기록에 재사용한다(NavGoalLogRecorder는
    // 로컬 NavigationService 시그널만 보므로 Pi 주도 복귀는 여기서 직접 기록).
    QVector<NavPose> m_pendingPiReturnPoses;
    int m_piReturnSessionId {-1};
    int m_piReturnLastMarkedIndex {-1};
    QVector<MissionQueueItem> m_queueItems;
    QVector<MissionHistoryEntry> m_historyEntries;
    MissionSnapshot m_snapshot;
    PendingAction m_pendingAction {PendingAction::None};
    QString m_pendingDivertQueueId;
    QString m_pendingReason;
    QDateTime m_lastRosHeartbeat;
    QString m_lastRosHeartbeatSource;
    bool m_nav2Connected {false};
    bool m_rosConnected {false};
    bool m_retryInProgress {false};
    bool m_goalAccepted {false};
    QTimer *m_elapsedTimer {nullptr};
    // 복귀·도킹 완료(Completed) 후 완료 체크마크를 30초간 보여주고 나서야
    // startNextQueueIfPossible()을 부르는 지연 타이머(finishMission() 참고).
    QTimer *m_completionResetTimer {nullptr};
    double m_lastRobotX {0.0};
    double m_lastRobotY {0.0};
    bool m_robotPoseValid {false};
    double m_initialDistanceMeters {-1.0};
    // observedButtonIds() 참고 — 최근 감지 순(맨 앞이 최신).
    QStringList m_observedButtonIds;
};

#endif // MISSIONCONTROLLER_H
