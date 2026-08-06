#ifndef MISSIONSTORAGESERVICE_H
#define MISSIONSTORAGESERVICE_H

#include "../models/missiontypes.h"
#include "../app/appsession.h"

#include <QObject>

class MissionStorageService : public QObject
{
    Q_OBJECT

public:
    explicit MissionStorageService(QObject *parent = nullptr);

    void setSessionContext(const AppSession &session);
    QVector<MissionQueueItem> loadQueue();
    QVector<MissionHistoryEntry> loadTodayHistory();
    void saveQueue(QVector<MissionQueueItem> &queueItems);
    void appendHistory(const MissionHistoryEntry &entry);
    QVector<MissionHistoryEntry> todayHistory() const;

private:
    void ensureLoaded();
    void ensureTable();
    int ensureDefaultRobotId();
    int ensureCallButtonId(const MissionQueueItem &item);
    int upsertMissionRow(MissionQueueItem &item);
    void syncMissionStatus(const MissionQueueItem &item);
    void insertSystemEvent(const MissionHistoryEntry &entry);
    void insertUserActionLog(const MissionHistoryEntry &entry);
    void insertConnectionStatusLog(const MissionHistoryEntry &entry);
    static bool isNumericId(const QString &text);
    static QString missionStatusForQueue(const MissionQueueItem &item);
    static MissionQueueStatus queueStatusFromMissionStatus(const QString &status);
    static QString actionTypeForEvent(const QString &eventType);
    static QJsonObject historyPayloadObject(const MissionHistoryEntry &entry);
    static MissionHistoryEntry historyEntryFromPayload(const QString &eventType, const QDateTime &occurredAt, const QString &payloadText);
    QString storagePath(const QString &name) const;
    QVector<MissionQueueItem> loadQueueFromJson() const;
    QVector<MissionHistoryEntry> loadHistoryFromJson() const;
    void saveQueueToJson(const QVector<MissionQueueItem> &queueItems) const;
    void saveHistoryToJson(const QVector<MissionHistoryEntry> &entries) const;

    bool m_loaded {false};
    bool m_dbReady {false};
    QVector<MissionHistoryEntry> m_historyCache;
    AppSession m_session;
    int m_robotId {0};
};

#endif // MISSIONSTORAGESERVICE_H
