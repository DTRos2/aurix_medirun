#ifndef TELEMETRYMODEL_H
#define TELEMETRYMODEL_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

enum class ConnectionState {
    Connected,
    Warning,
    Disconnected
};

enum class SeverityLevel {
    Info,
    Warning,
    Error
};

struct SystemEventItem
{
    QDateTime occurredAt;
    SeverityLevel severity {SeverityLevel::Info};
    QString eventType;
    QString missionId;
    QString message;
    bool synced {false};
};

struct TelemetrySnapshot
{
    QString robotName;
    QString missionId;
    QString missionState;
    QString destination;
    QString buttonId;
    QString mode;
    QString stopReason;
    QString batteryPercentText;
    int batteryPercent {-1};
    double batteryVoltage {0.0};
    bool batteryMessageReceived {false};
    QString positionText;
    QString networkText;
    QString rosText;
    QString dbText;
    QString remainingDistanceText;
    QString elapsedTimeText;
    QString errorCodeText;
    QString payloadText;
    QString connectionDelayText;
    QString autoResumeCountdownText;
    ConnectionState rosState {ConnectionState::Connected};
    ConnectionState networkState {ConnectionState::Connected};
    ConnectionState dbState {ConnectionState::Warning};
    QVector<SystemEventItem> recentEvents;
    QStringList autoResumeConditions;
    QVector<QPair<QString, QString>> deviceStates;
};

Q_DECLARE_METATYPE(TelemetrySnapshot)

#endif // TELEMETRYMODEL_H
