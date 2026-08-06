#include "telemetrylogrecorder.h"
#include "operationsoverviewservice.h"
#include "robotservice.h"

namespace {

constexpr int kBatteryWarnThreshold = 20;
constexpr int kBatteryCriticalThreshold = 10;

QString connectionStatusForRosState(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Connected:
        return QStringLiteral("connected");
    case ConnectionState::Warning:
        return QStringLiteral("reconnecting");
    case ConnectionState::Disconnected:
        return QStringLiteral("disconnected");
    }
    return QStringLiteral("disconnected");
}

} // namespace

TelemetryLogRecorder::TelemetryLogRecorder(RobotService &robotService, OperationsOverviewService &overviewService, QObject *parent)
    : QObject(parent)
    , m_robotService(robotService)
    , m_overviewService(overviewService)
{
}

void TelemetryLogRecorder::handleSnapshot(const TelemetrySnapshot &snapshot)
{
    const QVector<RobotRecord> robots = m_robotService.listRobots(false);
    if (robots.size() != 1) {
        // 어느 로봇의 텔레메트리인지 특정할 수 없으면 기록하지 않고, 다음에
        // 로봇이 정확히 1대가 됐을 때 상태를 새로 관찰하도록 초기화한다.
        m_hasLastRosState = false;
        m_hasLastBatteryBand = false;
        return;
    }

    const RobotRecord &robot = robots.first();

    if (!m_hasLastRosState || snapshot.rosState != m_lastRosState) {
        QString errorMessage;
        m_overviewService.recordConnectionStatus(
            robot.id, QStringLiteral("ros_bridge"), connectionStatusForRosState(snapshot.rosState), snapshot.rosText, errorMessage);
        m_hasLastRosState = true;
        m_lastRosState = snapshot.rosState;
    }

    if (!snapshot.batteryMessageReceived) {
        return;
    }

    BatteryBand band = BatteryBand::Normal;
    if (snapshot.batteryPercent < kBatteryCriticalThreshold) {
        band = BatteryBand::Critical;
    } else if (snapshot.batteryPercent < kBatteryWarnThreshold) {
        band = BatteryBand::Warn;
    }

    if (m_hasLastBatteryBand && band == m_lastBatteryBand) {
        return;
    }
    m_hasLastBatteryBand = true;
    m_lastBatteryBand = band;

    if (band == BatteryBand::Normal) {
        return;
    }

    QString errorMessage;
    if (band == BatteryBand::Critical) {
        m_overviewService.recordSystemEvent(
            QStringLiteral("battery_critical"),
            QStringLiteral("critical"),
            QStringLiteral("배터리 심각 (%1%) · %2").arg(snapshot.batteryPercent).arg(robot.name),
            errorMessage);
    } else {
        m_overviewService.recordSystemEvent(
            QStringLiteral("battery_low"),
            QStringLiteral("warn"),
            QStringLiteral("배터리 부족 (%1%) · %2").arg(snapshot.batteryPercent).arg(robot.name),
            errorMessage);
    }
}
