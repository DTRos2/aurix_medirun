#include "missionstorageservice.h"

#include "dbconnection.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>

#include <algorithm>
#include <utility>

namespace {

QString queueStatusToJson(MissionQueueStatus status)
{
    return missionQueueStatusText(status);
}

MissionQueueStatus queueStatusFromJson(const QString &text)
{
    if (text == QStringLiteral("ACTIVE")) {
        return MissionQueueStatus::Active;
    }
    if (text == QStringLiteral("COMPLETED")) {
        return MissionQueueStatus::Completed;
    }
    if (text == QStringLiteral("CANCELLED")) {
        return MissionQueueStatus::Cancelled;
    }
    if (text == QStringLiteral("DIVERTED")) {
        return MissionQueueStatus::Diverted;
    }
    if (text == QStringLiteral("FAILED")) {
        return MissionQueueStatus::Failed;
    }
    return MissionQueueStatus::Waiting;
}

QJsonObject queueToObject(const MissionQueueItem &item)
{
    QJsonObject object;
    object.insert(QStringLiteral("queue_id"), item.queueId);
    object.insert(QStringLiteral("waypoint_id"), item.waypointId);
    object.insert(QStringLiteral("waypoint_name"), item.waypointName);
    object.insert(QStringLiteral("requested_at"), item.requestedAt.toString(Qt::ISODate));
    object.insert(QStringLiteral("enqueue_order"), item.enqueueOrder);
    object.insert(QStringLiteral("status"), queueStatusToJson(item.status));
    object.insert(QStringLiteral("request_source"), item.requestSource);
    object.insert(QStringLiteral("mission_id"), item.missionId);
    object.insert(QStringLiteral("frame_id"), item.pose.frameId);
    object.insert(QStringLiteral("position_x"), item.pose.positionX);
    object.insert(QStringLiteral("position_y"), item.pose.positionY);
    object.insert(QStringLiteral("position_z"), item.pose.positionZ);
    object.insert(QStringLiteral("orientation_x"), item.pose.orientationX);
    object.insert(QStringLiteral("orientation_y"), item.pose.orientationY);
    object.insert(QStringLiteral("orientation_z"), item.pose.orientationZ);
    object.insert(QStringLiteral("orientation_w"), item.pose.orientationW);
    object.insert(QStringLiteral("yaw_radians"), item.pose.yawRadians);
    object.insert(QStringLiteral("valid"), item.pose.valid);
    return object;
}

MissionQueueItem queueFromObject(const QJsonObject &object)
{
    MissionQueueItem item;
    item.queueId = object.value(QStringLiteral("queue_id")).toString();
    item.waypointId = object.value(QStringLiteral("waypoint_id")).toString();
    item.waypointName = object.value(QStringLiteral("waypoint_name")).toString();
    item.requestedAt = QDateTime::fromString(object.value(QStringLiteral("requested_at")).toString(), Qt::ISODate);
    item.enqueueOrder = object.value(QStringLiteral("enqueue_order")).toInt();
    item.status = queueStatusFromJson(object.value(QStringLiteral("status")).toString());
    item.requestSource = object.value(QStringLiteral("request_source")).toString();
    item.missionId = object.value(QStringLiteral("mission_id")).toString();
    item.pose.frameId = object.value(QStringLiteral("frame_id")).toString(QStringLiteral("map"));
    item.pose.positionX = object.value(QStringLiteral("position_x")).toDouble();
    item.pose.positionY = object.value(QStringLiteral("position_y")).toDouble();
    item.pose.positionZ = object.value(QStringLiteral("position_z")).toDouble();
    item.pose.orientationX = object.value(QStringLiteral("orientation_x")).toDouble();
    item.pose.orientationY = object.value(QStringLiteral("orientation_y")).toDouble();
    item.pose.orientationZ = object.value(QStringLiteral("orientation_z")).toDouble();
    item.pose.orientationW = object.value(QStringLiteral("orientation_w")).toDouble(1.0);
    item.pose.yawRadians = object.value(QStringLiteral("yaw_radians")).toDouble();
    item.pose.valid = object.value(QStringLiteral("valid")).toBool();
    return item;
}

QJsonObject historyToObject(const MissionHistoryEntry &entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("history_id"), entry.historyId);
    object.insert(QStringLiteral("occurred_at"), entry.occurredAt.toString(Qt::ISODate));
    object.insert(QStringLiteral("event_type"), entry.eventType);
    object.insert(QStringLiteral("waypoint_id"), entry.waypointId);
    object.insert(QStringLiteral("waypoint_name"), entry.waypointName);
    object.insert(QStringLiteral("mission_id"), entry.missionId);
    object.insert(QStringLiteral("queue_id"), entry.queueId);
    object.insert(QStringLiteral("result"), entry.result);
    object.insert(QStringLiteral("detail_message"), entry.detailMessage);
    return object;
}

MissionHistoryEntry historyFromObject(const QJsonObject &object)
{
    MissionHistoryEntry entry;
    entry.historyId = object.value(QStringLiteral("history_id")).toString();
    entry.occurredAt = QDateTime::fromString(object.value(QStringLiteral("occurred_at")).toString(), Qt::ISODate);
    entry.eventType = object.value(QStringLiteral("event_type")).toString();
    entry.waypointId = object.value(QStringLiteral("waypoint_id")).toString();
    entry.waypointName = object.value(QStringLiteral("waypoint_name")).toString();
    entry.missionId = object.value(QStringLiteral("mission_id")).toString();
    entry.queueId = object.value(QStringLiteral("queue_id")).toString();
    entry.result = object.value(QStringLiteral("result")).toString();
    entry.detailMessage = object.value(QStringLiteral("detail_message")).toString();
    return entry;
}

QString severityForResult(const QString &result)
{
    const QString normalized = result.toLower();
    if (normalized.contains(QStringLiteral("failed")) || normalized.contains(QStringLiteral("error"))) {
        return QStringLiteral("error");
    }
    if (normalized.contains(QStringLiteral("cancel")) || normalized.contains(QStringLiteral("divert")) || normalized.contains(QStringLiteral("requested"))) {
        return QStringLiteral("warn");
    }
    return QStringLiteral("info");
}

} // namespace

MissionStorageService::MissionStorageService(QObject *parent)
    : QObject(parent)
{
}

void MissionStorageService::setSessionContext(const AppSession &session)
{
    m_session = session;
}

QVector<MissionQueueItem> MissionStorageService::loadQueue()
{
    ensureLoaded();
    if (m_dbReady) {
        QVector<MissionQueueItem> items;
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "SELECT m.id, cb.code, cb.zone_name, m.created_at, m.status, cb.pos_x, cb.pos_y "
            "FROM mission m "
            "JOIN call_button cb ON cb.id = m.call_button_id "
            "WHERE m.status IN ('queued', 'dispatched', 'navigating', 'replanning', 'arrived', 'dispensing') "
            "ORDER BY m.created_at ASC"));
        if (query.exec()) {
            int order = 1;
            while (query.next()) {
                MissionQueueItem item;
                const QString missionId = query.value(0).toString();
                item.queueId = missionId;
                item.missionId = missionId;
                item.waypointId = query.value(1).toString();
                item.waypointName = query.value(2).toString();
                item.requestedAt = query.value(3).toDateTime();
                item.enqueueOrder = order++;
                item.status = queueStatusFromMissionStatus(query.value(4).toString());
                item.requestSource = QStringLiteral("db");
                item.pose.frameId = QStringLiteral("map");
                item.pose.positionX = query.value(5).toDouble();
                item.pose.positionY = query.value(6).toDouble();
                item.pose.valid = !query.value(5).isNull() && !query.value(6).isNull();
                items.append(item);
            }
        }
        return items;
    }
    return loadQueueFromJson();
}

QVector<MissionHistoryEntry> MissionStorageService::loadTodayHistory()
{
    ensureLoaded();
    if (m_dbReady) {
        m_historyCache.clear();
        const QDateTime start = QDateTime(QDate::currentDate().startOfDay());
        const QDateTime end = QDateTime(QDate::currentDate().addDays(1).startOfDay());
        QSqlQuery query;
        query.prepare(QStringLiteral(
            "SELECT event_type, created_at, message "
            "FROM system_event "
            "WHERE created_at >= :start AND created_at < :end "
            "ORDER BY created_at DESC"));
        query.bindValue(QStringLiteral(":start"), start);
        query.bindValue(QStringLiteral(":end"), end);
        if (query.exec()) {
            while (query.next()) {
                m_historyCache.append(historyEntryFromPayload(query.value(0).toString(),
                                                              query.value(1).toDateTime(),
                                                              query.value(2).toString()));
            }
        }
        return m_historyCache;
    }

    m_historyCache = loadHistoryFromJson();
    QVector<MissionHistoryEntry> todayEntries;
    const QDate today = QDate::currentDate();
    for (const MissionHistoryEntry &entry : std::as_const(m_historyCache)) {
        if (entry.occurredAt.date() == today) {
            todayEntries.append(entry);
        }
    }
    std::sort(todayEntries.begin(), todayEntries.end(), [](const MissionHistoryEntry &lhs, const MissionHistoryEntry &rhs) {
        return lhs.occurredAt > rhs.occurredAt;
    });
    return todayEntries;
}

void MissionStorageService::saveQueue(QVector<MissionQueueItem> &queueItems)
{
    ensureLoaded();
    if (m_dbReady) {
        for (MissionQueueItem &item : queueItems) {
            const int missionDbId = upsertMissionRow(item);
            if (missionDbId > 0) {
                item.missionId = QString::number(missionDbId);
                item.queueId = QString::number(missionDbId);
                syncMissionStatus(item);
            }
        }
        return;
    }
    saveQueueToJson(queueItems);
}

void MissionStorageService::appendHistory(const MissionHistoryEntry &entry)
{
    ensureLoaded();
    if (m_dbReady) {
        insertSystemEvent(entry);
        insertUserActionLog(entry);
        insertConnectionStatusLog(entry);
        return;
    }

    m_historyCache = loadHistoryFromJson();
    m_historyCache.prepend(entry);
    saveHistoryToJson(m_historyCache);
}

QVector<MissionHistoryEntry> MissionStorageService::todayHistory() const
{
    return m_historyCache;
}

void MissionStorageService::ensureLoaded()
{
    if (m_loaded) {
        return;
    }
    ensureTable();
    if (m_dbReady) {
        m_robotId = ensureDefaultRobotId();
    }
    m_loaded = true;
}

void MissionStorageService::ensureTable()
{
    QString errorMessage;
    m_dbReady = openDbConnection(errorMessage);
}

int MissionStorageService::ensureDefaultRobotId()
{
    QSqlQuery findQuery;
    findQuery.prepare(QStringLiteral("SELECT id FROM robot WHERE name = :name"));
    findQuery.bindValue(QStringLiteral(":name"), QStringLiteral("medirun_gui"));
    if (findQuery.exec() && findQuery.next()) {
        return findQuery.value(0).toInt();
    }

    QSqlQuery insertQuery;
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO robot (name, model) VALUES (:name, :model) RETURNING id"));
    insertQuery.bindValue(QStringLiteral(":name"), QStringLiteral("medirun_gui"));
    insertQuery.bindValue(QStringLiteral(":model"), QStringLiteral("turtlebot3_burger"));
    if (insertQuery.exec() && insertQuery.next()) {
        return insertQuery.value(0).toInt();
    }
    return 0;
}

int MissionStorageService::ensureCallButtonId(const MissionQueueItem &item)
{
    QSqlQuery findQuery;
    findQuery.prepare(QStringLiteral("SELECT id FROM call_button WHERE code = :code"));
    findQuery.bindValue(QStringLiteral(":code"), item.waypointId);
    if (findQuery.exec() && findQuery.next()) {
        return findQuery.value(0).toInt();
    }

    QSqlQuery insertQuery;
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO call_button (code, zone_name, pos_x, pos_y) "
        "VALUES (:code, :zoneName, :posX, :posY) RETURNING id"));
    insertQuery.bindValue(QStringLiteral(":code"), item.waypointId);
    insertQuery.bindValue(QStringLiteral(":zoneName"), item.waypointName.isEmpty() ? item.waypointId : item.waypointName);
    insertQuery.bindValue(QStringLiteral(":posX"), item.pose.valid ? QVariant(item.pose.positionX) : QVariant());
    insertQuery.bindValue(QStringLiteral(":posY"), item.pose.valid ? QVariant(item.pose.positionY) : QVariant());
    if (insertQuery.exec() && insertQuery.next()) {
        return insertQuery.value(0).toInt();
    }
    return 0;
}

int MissionStorageService::upsertMissionRow(MissionQueueItem &item)
{
    const int callButtonId = ensureCallButtonId(item);
    if (callButtonId <= 0) {
        return 0;
    }

    if (isNumericId(item.missionId)) {
        return item.missionId.toInt();
    }

    QSqlQuery insertQuery;
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO mission (robot_id, call_button_id, status, created_at) "
        "VALUES (:robotId, :callButtonId, :status, :createdAt) RETURNING id"));
    insertQuery.bindValue(QStringLiteral(":robotId"), m_robotId > 0 ? QVariant(m_robotId) : QVariant());
    insertQuery.bindValue(QStringLiteral(":callButtonId"), callButtonId);
    insertQuery.bindValue(QStringLiteral(":status"), missionStatusForQueue(item));
    insertQuery.bindValue(QStringLiteral(":createdAt"), item.requestedAt.isValid() ? item.requestedAt : QDateTime::currentDateTime());
    if (insertQuery.exec() && insertQuery.next()) {
        return insertQuery.value(0).toInt();
    }
    return 0;
}

void MissionStorageService::syncMissionStatus(const MissionQueueItem &item)
{
    if (!isNumericId(item.missionId)) {
        return;
    }

    const QString status = missionStatusForQueue(item);
    QSqlQuery updateQuery;
    updateQuery.prepare(QStringLiteral(
        "UPDATE mission SET status = :status, robot_id = :robotId, "
        "dispatched_at = CASE "
        "  WHEN :status IN ('dispatched', 'navigating', 'replanning', 'arrived', 'dispensing', 'completed', 'cancelled', 'failed') "
        "       AND dispatched_at IS NULL THEN NOW() ELSE dispatched_at END, "
        "arrived_at = CASE "
        "  WHEN :status IN ('arrived', 'dispensing', 'completed') AND arrived_at IS NULL THEN NOW() ELSE arrived_at END, "
        "ended_at = CASE "
        "  WHEN :status IN ('completed', 'cancelled', 'failed') THEN NOW() ELSE ended_at END "
        "WHERE id = :id"));
    updateQuery.bindValue(QStringLiteral(":status"), status);
    updateQuery.bindValue(QStringLiteral(":robotId"), m_robotId > 0 ? QVariant(m_robotId) : QVariant());
    updateQuery.bindValue(QStringLiteral(":id"), item.missionId.toInt());
    updateQuery.exec();
}

void MissionStorageService::insertSystemEvent(const MissionHistoryEntry &entry)
{
    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO system_event (created_at, event_type, severity, message) "
        "VALUES (:createdAt, :eventType, :severity, :message)"));
    query.bindValue(QStringLiteral(":createdAt"), entry.occurredAt);
    query.bindValue(QStringLiteral(":eventType"), entry.eventType);
    query.bindValue(QStringLiteral(":severity"), severityForResult(entry.result));
    query.bindValue(QStringLiteral(":message"), QString::fromUtf8(QJsonDocument(historyPayloadObject(entry)).toJson(QJsonDocument::Compact)));
    query.exec();
}

void MissionStorageService::insertUserActionLog(const MissionHistoryEntry &entry)
{
    const QString actionType = actionTypeForEvent(entry.eventType);
    if (actionType.isEmpty()) {
        return;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO user_action_log (robot_id, user_id, mission_id, action_type, payload, issued_by, occurred_at) "
        "VALUES (:robotId, NULL, :missionId, :actionType, CAST(:payload AS JSONB), :issuedBy, :occurredAt)"));
    query.bindValue(QStringLiteral(":robotId"), m_robotId > 0 ? QVariant(m_robotId) : QVariant());
    query.bindValue(QStringLiteral(":missionId"), isNumericId(entry.missionId) ? QVariant(entry.missionId.toInt()) : QVariant());
    query.bindValue(QStringLiteral(":actionType"), actionType);
    query.bindValue(QStringLiteral(":payload"), QString::fromUtf8(QJsonDocument(historyPayloadObject(entry)).toJson(QJsonDocument::Compact)));
    query.bindValue(QStringLiteral(":issuedBy"), m_session.username);
    query.bindValue(QStringLiteral(":occurredAt"), entry.occurredAt);
    query.exec();
}

void MissionStorageService::insertConnectionStatusLog(const MissionHistoryEntry &entry)
{
    if (!entry.eventType.startsWith(QStringLiteral("ROS_RETRY"))) {
        return;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO connection_status_log (robot_id, target, status, detail, changed_at) "
        "VALUES (:robotId, :target, :status, :detail, :changedAt)"));
    query.bindValue(QStringLiteral(":robotId"), m_robotId > 0 ? QVariant(m_robotId) : QVariant());
    query.bindValue(QStringLiteral(":target"), QStringLiteral("ros_bridge"));
    query.bindValue(QStringLiteral(":status"),
                    entry.eventType.endsWith(QStringLiteral("STARTED")) ? QStringLiteral("reconnecting")
                    : entry.result.compare(QStringLiteral("SUCCEEDED"), Qt::CaseInsensitive) == 0 ? QStringLiteral("connected")
                                                                                                   : QStringLiteral("error"));
    query.bindValue(QStringLiteral(":detail"), entry.detailMessage);
    query.bindValue(QStringLiteral(":changedAt"), entry.occurredAt);
    query.exec();
}

bool MissionStorageService::isNumericId(const QString &text)
{
    bool ok = false;
    text.toInt(&ok);
    return ok;
}

QString MissionStorageService::missionStatusForQueue(const MissionQueueItem &item)
{
    switch (item.status) {
    case MissionQueueStatus::Waiting:
        return QStringLiteral("queued");
    case MissionQueueStatus::Active:
        return QStringLiteral("navigating");
    case MissionQueueStatus::Completed:
        return QStringLiteral("completed");
    case MissionQueueStatus::Cancelled:
        return QStringLiteral("cancelled");
    case MissionQueueStatus::Diverted:
        return QStringLiteral("cancelled");
    case MissionQueueStatus::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("queued");
}

MissionQueueStatus MissionStorageService::queueStatusFromMissionStatus(const QString &status)
{
    if (status == QStringLiteral("queued")) {
        return MissionQueueStatus::Waiting;
    }
    if (status == QStringLiteral("dispatched") || status == QStringLiteral("navigating")
        || status == QStringLiteral("replanning") || status == QStringLiteral("arrived")
        || status == QStringLiteral("dispensing")) {
        return MissionQueueStatus::Active;
    }
    if (status == QStringLiteral("completed")) {
        return MissionQueueStatus::Completed;
    }
    if (status == QStringLiteral("cancelled")) {
        return MissionQueueStatus::Cancelled;
    }
    if (status == QStringLiteral("failed")) {
        return MissionQueueStatus::Failed;
    }
    return MissionQueueStatus::Waiting;
}

QString MissionStorageService::actionTypeForEvent(const QString &eventType)
{
    if (eventType == QStringLiteral("MISSION_CANCEL_REQUESTED") || eventType == QStringLiteral("MISSION_CANCEL_IGNORED")) {
        return QStringLiteral("mission_cancel");
    }
    if (eventType == QStringLiteral("RETURN_HOME_REQUESTED") || eventType == QStringLiteral("RETURN_HOME_STARTED")) {
        return QStringLiteral("mission_return_home");
    }
    if (eventType == QStringLiteral("ROS_RETRY_STARTED") || eventType == QStringLiteral("ROS_RETRY_SUCCEEDED")
        || eventType == QStringLiteral("ROS_RETRY_FAILED")) {
        return QStringLiteral("mission_retry");
    }
    if (eventType == QStringLiteral("MISSION_DIVERT_REQUESTED") || eventType == QStringLiteral("MISSION_DIVERTED_STARTED")) {
        return QStringLiteral("mission_accept");
    }
    return QString();
}

QJsonObject MissionStorageService::historyPayloadObject(const MissionHistoryEntry &entry)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("history_id"), entry.historyId);
    payload.insert(QStringLiteral("occurred_at"), entry.occurredAt.toString(Qt::ISODate));
    payload.insert(QStringLiteral("waypoint_id"), entry.waypointId);
    payload.insert(QStringLiteral("waypoint_name"), entry.waypointName);
    payload.insert(QStringLiteral("mission_id"), entry.missionId);
    payload.insert(QStringLiteral("queue_id"), entry.queueId);
    payload.insert(QStringLiteral("result"), entry.result);
    payload.insert(QStringLiteral("detail_message"), entry.detailMessage);
    return payload;
}

MissionHistoryEntry MissionStorageService::historyEntryFromPayload(const QString &eventType,
                                                                   const QDateTime &occurredAt,
                                                                   const QString &payloadText)
{
    MissionHistoryEntry entry;
    entry.eventType = eventType;
    entry.occurredAt = occurredAt;

    const QJsonDocument doc = QJsonDocument::fromJson(payloadText.toUtf8());
    if (!doc.isObject()) {
        entry.detailMessage = payloadText;
        entry.result = QStringLiteral("INFO");
        return entry;
    }

    const QJsonObject object = doc.object();
    entry.historyId = object.value(QStringLiteral("history_id")).toString();
    entry.waypointId = object.value(QStringLiteral("waypoint_id")).toString();
    entry.waypointName = object.value(QStringLiteral("waypoint_name")).toString();
    entry.missionId = object.value(QStringLiteral("mission_id")).toString();
    entry.queueId = object.value(QStringLiteral("queue_id")).toString();
    entry.result = object.value(QStringLiteral("result")).toString();
    entry.detailMessage = object.value(QStringLiteral("detail_message")).toString();
    if (entry.historyId.isEmpty()) {
        entry.historyId = QStringLiteral("%1_%2").arg(eventType, occurredAt.toString(Qt::ISODate));
    }
    return entry;
}

QString MissionStorageService::storagePath(const QString &name) const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(root);
    dir.mkpath(QStringLiteral("."));
    return dir.filePath(name);
}

QVector<MissionQueueItem> MissionStorageService::loadQueueFromJson() const
{
    QVector<MissionQueueItem> items;
    QFile file(storagePath(QStringLiteral("mission_queue.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return items;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue &value : doc.array()) {
        items.append(queueFromObject(value.toObject()));
    }
    return items;
}

QVector<MissionHistoryEntry> MissionStorageService::loadHistoryFromJson() const
{
    QVector<MissionHistoryEntry> entries;
    QFile file(storagePath(QStringLiteral("mission_history.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return entries;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    for (const QJsonValue &value : doc.array()) {
        entries.append(historyFromObject(value.toObject()));
    }
    return entries;
}

void MissionStorageService::saveQueueToJson(const QVector<MissionQueueItem> &queueItems) const
{
    QJsonArray array;
    for (const MissionQueueItem &item : queueItems) {
        array.append(queueToObject(item));
    }
    QFile file(storagePath(QStringLiteral("mission_queue.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void MissionStorageService::saveHistoryToJson(const QVector<MissionHistoryEntry> &entries) const
{
    QJsonArray array;
    for (const MissionHistoryEntry &entry : entries) {
        array.append(historyToObject(entry));
    }
    QFile file(storagePath(QStringLiteral("mission_history.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}
