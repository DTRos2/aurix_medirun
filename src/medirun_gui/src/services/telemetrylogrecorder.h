#ifndef TELEMETRYLOGRECORDER_H
#define TELEMETRYLOGRECORDER_H

#include "../models/telemetrymodel.h"

#include <QObject>

class RobotService;
class OperationsOverviewService;

// TelemetryProvider::snapshotUpdated를 구독해, 등록된 로봇이 정확히 1대일 때만
// (여러 대면 어느 로봇 데이터인지 특정할 수 없어 기록하지 않음) 상태 변화를
// DB에 기록한다:
//  - ROS 연결 상태가 바뀔 때마다 connection_status_log 1행
//  - 배터리가 20%/10% 미만으로 처음 진입할 때 system_event 1행 (매 스냅샷 아님)
class TelemetryLogRecorder : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryLogRecorder(RobotService &robotService, OperationsOverviewService &overviewService, QObject *parent = nullptr);

public slots:
    void handleSnapshot(const TelemetrySnapshot &snapshot);

private:
    enum class BatteryBand { Normal, Warn, Critical };

    RobotService &m_robotService;
    OperationsOverviewService &m_overviewService;

    bool m_hasLastRosState {false};
    ConnectionState m_lastRosState {ConnectionState::Disconnected};
    bool m_hasLastBatteryBand {false};
    BatteryBand m_lastBatteryBand {BatteryBand::Normal};
};

#endif // TELEMETRYLOGRECORDER_H
