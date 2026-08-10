#ifndef MISSIONCONTROLLER_H
#define MISSIONCONTROLLER_H

#include "../app/appsession.h"
#include "../models/missiontypes.h"

#include <QObject>
#include <QTimer>

class NavigationService;
class WaypointRepository;
class MissionStorageService;
class MissionRosInterface;

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

public slots:
    void requestRetry();
    void requestReturnHome();
    void requestMissionCancel();
    void requestQueueCancel(const QString &queueId);
    void requestQueueDivert(const QString &queueId);

signals:
    void missionChanged();
    void queueChanged();
    void historyChanged();
    void connectionStateChanged();
    void retryStateChanged();

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
    bool resolveHomeWaypoint(WaypointDef &homeWaypoint, WaypointPose &homePose, QString &errorMessage) const;
    void finishMission(MissionState finalState, const QString &reason, const QString &historyType, const QString &historyResult);
    void markQueueStatus(const QString &queueId, MissionQueueStatus status);
    void removeQueueItem(const QString &queueId);
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
    double m_lastRobotX {0.0};
    double m_lastRobotY {0.0};
    bool m_robotPoseValid {false};
    double m_initialDistanceMeters {-1.0};
};

#endif // MISSIONCONTROLLER_H
