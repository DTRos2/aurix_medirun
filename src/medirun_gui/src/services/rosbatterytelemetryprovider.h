#ifndef ROSBATTERYTELEMETRYPROVIDER_H
#define ROSBATTERYTELEMETRYPROVIDER_H

#include "telemetryprovider.h"

#include <QTimer>

#include <atomic>
#include <mutex>
#include <memory>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/battery_state.hpp>

class RosBatteryTelemetryProvider : public TelemetryProvider
{
    Q_OBJECT

public:
    explicit RosBatteryTelemetryProvider(QObject *parent = nullptr);
    ~RosBatteryTelemetryProvider() override;

public slots:
    void start() override;

private slots:
    void checkBatteryTimeout();

private:
    void stopExecutor();
    void handleBatteryState(const sensor_msgs::msg::BatteryState::SharedPtr msg);
    TelemetrySnapshot buildWaitingSnapshot() const;
    TelemetrySnapshot buildDisconnectedSnapshot() const;
    TelemetrySnapshot buildBatterySnapshot(int batteryPercent, double voltage) const;
    double normalizeBatteryPercent(double percentage) const;
    double filterBatteryPercent(double batteryPercent);
    int stabilizeDisplayedPercent(double filteredPercent);
    void refreshDbState();
    void applyDbState(TelemetrySnapshot &snapshot) const;
    void emitSnapshot(TelemetrySnapshot snapshot);

    QTimer *m_timeoutTimer;
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Subscription<sensor_msgs::msg::BatteryState>::SharedPtr m_subscription;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::thread m_spinThread;
    std::atomic<bool> m_started;
    std::atomic<bool> m_connectionLost;
    std::atomic<bool> m_hasReceivedMessage;
    std::atomic<qint64> m_lastMessageMs;
    std::mutex m_batteryStateMutex;
    double m_filteredBatteryPercent;
    qint64 m_filteredBatteryPercentTimeMs;
    int m_displayedBatteryPercent;
    bool m_hasFilteredBatteryPercent;
    bool m_hasDisplayedBatteryPercent;
    TelemetrySnapshot m_lastSnapshot;
    // QSqlDatabase는 연결을 만든 스레드(GUI 스레드)에서만 조회해야 하므로,
    // 배터리 콜백(ROS spin 스레드)에서도 안전하게 읽을 수 있도록 GUI
    // 스레드(checkBatteryTimeout)에서만 갱신하는 캐시로 들고 있는다.
    std::atomic<bool> m_dbConnected;
};

#endif // ROSBATTERYTELEMETRYPROVIDER_H
