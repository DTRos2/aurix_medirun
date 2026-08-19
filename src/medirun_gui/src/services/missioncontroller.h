#ifndef MISSIONCONTROLLER_H
#define MISSIONCONTROLLER_H

#include "../app/appsession.h"
#include "../models/missiontypes.h"

#include <QObject>
#include <QStringList>
#include <QTimer>

class NavigationService;
class WaypointRepository;
class MissionStorageService;
class MissionRosInterface;
class SensorTelemetryProvider;

class MissionController : public QObject
{
    Q_OBJECT

public:
    explicit MissionController(const AppSession &session,
                               NavigationService *navigationService,
                               WaypointRepository *waypointRepository,
                               MissionStorageService *storageService,
                               MissionRosInterface *rosInterface,
                               QObject *parent = nullptr);

    // MissionController가 SensorTelemetryProvider보다 먼저 생성돼(생성자 인자로
    // 못 받음) late-binding으로 연결한다 — setRobotService 등과 동일한 패턴.
    // AED 반출(홀센서) 감지로 도착 후 자동 복귀를 트리거하는 데 쓴다.
    void setSensorTelemetryProvider(SensorTelemetryProvider *provider);

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
    void handlePayloadLoadedChanged(bool loaded);
    void handlePayloadRemovedDelayElapsed();
    void handleForcedReturnTimeout();
    void handleReminderTick();
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
    void startMissionForQueue(const QString &queueId, bool divertedFromActiveMission);
    bool beginReturnHomeFlow(const QString &reason);
    void beginArrivedWait();
    void stopArrivalTimers();
    bool resolveHomeWaypoint(WaypointDef &homeWaypoint, WaypointPose &homePose, QString &errorMessage) const;
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
    SensorTelemetryProvider *m_sensorTelemetryProvider {nullptr};
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
    // 도착(Arrived) 상태 동안만 쓰는 타이머 3개 — SR-F-010/TC-12 기준
    // (AED 반출 감지 15초 후 복귀 / 미반출 시 60초마다 안내 / 180초 경과 시 강제 복귀).
    QTimer *m_payloadRemovedDelayTimer {nullptr};
    QTimer *m_forcedReturnTimer {nullptr};
    QTimer *m_reminderTimer {nullptr};
    double m_lastRobotX {0.0};
    double m_lastRobotY {0.0};
    bool m_robotPoseValid {false};
    double m_initialDistanceMeters {-1.0};
    // observedButtonIds() 참고 — 최근 감지 순(맨 앞이 최신).
    QStringList m_observedButtonIds;
};

#endif // MISSIONCONTROLLER_H
