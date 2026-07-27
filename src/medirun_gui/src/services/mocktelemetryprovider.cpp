#include "mocktelemetryprovider.h"

#include <QDateTime>
#include <QTimer>

namespace {
const QStringList kMissionStates = {
    QStringLiteral("IDLE"),
    QStringLiteral("VALIDATING"),
    QStringLiteral("NAVIGATING"),
    QStringLiteral("ARRIVED"),
    QStringLiteral("RETURNING_HOME"),
    QStringLiteral("DOCKING"),
    QStringLiteral("FAILED"),
    QStringLiteral("EMERGENCY_STOP"),
    QStringLiteral("CONNECTION_LOST")
};
}

MockTelemetryProvider::MockTelemetryProvider(QObject *parent)
    : TelemetryProvider(parent)
    , m_timer(new QTimer(this))
    , m_step(0)
{
    m_timer->setInterval(1200);
    connect(m_timer, &QTimer::timeout, this, &MockTelemetryProvider::publishNextSnapshot);
}

void MockTelemetryProvider::start()
{
    publishNextSnapshot();
    m_timer->start();
}

void MockTelemetryProvider::publishNextSnapshot()
{
    emit snapshotUpdated(buildSnapshot());
    ++m_step;
}

TelemetrySnapshot MockTelemetryProvider::buildSnapshot() const
{
    TelemetrySnapshot snapshot;
    snapshot.robotName = QStringLiteral("TB3-01");
    snapshot.missionId = QStringLiteral("MR-20260725-001");
    snapshot.missionState = kMissionStates.at(m_step % kMissionStates.size());
    snapshot.destination = QStringLiteral("DEMO_WP_0%1").arg((m_step % 3) + 1);
    snapshot.buttonId = QStringLiteral("BTN_0%1").arg((m_step % 3) + 1);
    snapshot.mode = (m_step % 8 == 7) ? QStringLiteral("E-STOP") : QStringLiteral("AUTO");
    snapshot.stopReason = (m_step % 8 == 7) ? QStringLiteral("비상 정지 활성") : QStringLiteral("--");
    snapshot.batteryPercentText = QStringLiteral("%1%").arg(82 - (m_step % 20));
    snapshot.positionText = QStringLiteral("X: 12.%1 / Y: 4.%2").arg(m_step % 10).arg((m_step + 3) % 10);
    snapshot.networkText = (m_step % 6 == 5) ? QStringLiteral("지연 2.3s") : QStringLiteral("정상");
    snapshot.rosText = (m_step % 6 == 5) ? QStringLiteral("미수신") : QStringLiteral("연결됨");
    snapshot.dbText = (m_step % 4 == 3) ? QStringLiteral("지연") : QStringLiteral("연결됨");
    snapshot.remainingDistanceText = QStringLiteral("%1 m").arg(qMax(0, 18 - (m_step % 12)));
    snapshot.elapsedTimeText = QStringLiteral("%1초").arg(15 + (m_step * 7));
    snapshot.errorCodeText = snapshot.missionState == QStringLiteral("FAILED") ? QStringLiteral("NAV_TIMEOUT") : QStringLiteral("--");
    snapshot.payloadText = (m_step % 5 == 4) ? QStringLiteral("AED 반출") : QStringLiteral("AED 적재");
    snapshot.connectionDelayText = (m_step % 6 == 5) ? QStringLiteral("2300 ms") : QStringLiteral("120 ms");
    snapshot.autoResumeCountdownText = QStringLiteral("%1.0 s").arg(qMax(0, 3 - (m_step % 4)));
    snapshot.rosState = (m_step % 6 == 5) ? ConnectionState::Disconnected : ConnectionState::Connected;
    snapshot.networkState = (m_step % 6 == 4) ? ConnectionState::Warning :
        ((m_step % 6 == 5) ? ConnectionState::Disconnected : ConnectionState::Connected);
    snapshot.dbState = (m_step % 4 == 3) ? ConnectionState::Warning : ConnectionState::Connected;
    snapshot.autoResumeConditions = {
        QStringLiteral("E-Stop 해제"),
        QStringLiteral("전방 LiDAR 거리 0.40m 이상"),
        QStringLiteral("하단 초음파 거리 0.15m 이상"),
        QStringLiteral("안전 조건 연속 유지 3초"),
        QStringLiteral("AMCL 위치 유효"),
        QStringLiteral("배터리 20% 이상")
    };
    snapshot.deviceStates = {
        {QStringLiteral("LiDAR"), QStringLiteral("정상")},
        {QStringLiteral("Nav2"), snapshot.missionState == QStringLiteral("FAILED") ? QStringLiteral("오류") : QStringLiteral("정상")},
        {QStringLiteral("AMCL"), QStringLiteral("정상")},
        {QStringLiteral("초음파"), QStringLiteral("정상")},
        {QStringLiteral("홀센서/AED"), snapshot.payloadText},
        {QStringLiteral("카메라"), QStringLiteral("정상")},
        {QStringLiteral("ArUco"), QStringLiteral("대기")},
        {QStringLiteral("Local DB"), snapshot.dbText},
        {QStringLiteral("ROS"), snapshot.rosText},
        {QStringLiteral("Network"), snapshot.networkText}
    };
    snapshot.recentEvents = {
        buildEvent(QStringLiteral("MISSION"), QStringLiteral("미션 상태가 %1 로 변경되었습니다.").arg(snapshot.missionState), SeverityLevel::Info),
        buildEvent(QStringLiteral("SAFETY"), snapshot.stopReason, snapshot.missionState == QStringLiteral("EMERGENCY_STOP") ? SeverityLevel::Error : SeverityLevel::Warning),
        buildEvent(QStringLiteral("NETWORK"), snapshot.networkText, snapshot.networkState == ConnectionState::Disconnected ? SeverityLevel::Error : SeverityLevel::Warning)
    };
    return snapshot;
}

SystemEventItem MockTelemetryProvider::buildEvent(const QString &type, const QString &message, SeverityLevel severity) const
{
    SystemEventItem event;
    event.occurredAt = QDateTime::currentDateTime();
    event.severity = severity;
    event.eventType = type;
    event.missionId = QStringLiteral("MR-20260725-001");
    event.message = message;
    event.synced = (m_step % 3 != 0);
    return event;
}
