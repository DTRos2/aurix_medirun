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
    // Pi에서 도는 robot_audio_output(audio_output_node)의 /audio/command로
    // TTS 문장을 발행한다 — bringup의 "주행을 시작합니다" 안내와 동일한 방식
    // (AudioCommand::TYPE_TTS, ko, 1회). Pi가 오프라인이면 그냥 유실된다.
    void publishAudioAnnouncement(const QString &text);
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
