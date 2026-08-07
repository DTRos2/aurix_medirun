#ifndef MISSIONROSINTERFACE_H
#define MISSIONROSINTERFACE_H

#include <QObject>
#include <QDateTime>

#include <memory>

class MissionRosInterface : public QObject
{
    Q_OBJECT

public:
    explicit MissionRosInterface(QObject *parent = nullptr);
    ~MissionRosInterface() override;

public slots:
    void publishMissionCommand(const QString &command,
                               const QString &missionId,
                               const QString &queueId,
                               const QString &waypointId);
    void retryCommunication();

signals:
    void dispatchRequested(const QString &buttonId,
                           const QString &requestSource,
                           const QString &missionId,
                           const QDateTime &requestedAt);
    void missionHeartbeat(const QString &source, const QDateTime &receivedAt);
    void communicationRetryFinished(bool success, const QString &detail);
    void logEvent(const QString &level, const QString &message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // MISSIONROSINTERFACE_H
