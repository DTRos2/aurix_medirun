#ifndef ROSSENSORTELEMETRYPROVIDER_H
#define ROSSENSORTELEMETRYPROVIDER_H

#include "sensortelemetryprovider.h"

#include <QTimer>

#include <atomic>
#include <memory>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/int32.hpp>

class RosSensorTelemetryProvider : public SensorTelemetryProvider
{
    Q_OBJECT

public:
    explicit RosSensorTelemetryProvider(QObject *parent = nullptr);
    ~RosSensorTelemetryProvider() override;

public slots:
    void start() override;

private slots:
    void checkTimeout();

private:
    void stopExecutor();

    QTimer *m_timeoutTimer;
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr m_frontDistanceSub;
    rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr m_rearDistanceSub;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr m_payloadLoadedSub;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::thread m_spinThread;
    std::atomic<bool> m_started;
    std::atomic<bool> m_connected;
    std::atomic<qint64> m_lastMessageMs;
};

#endif // ROSSENSORTELEMETRYPROVIDER_H
