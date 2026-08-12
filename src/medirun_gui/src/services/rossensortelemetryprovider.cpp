#include "rossensortelemetryprovider.h"

#include <QDateTime>

namespace {
constexpr int kMessageTimeoutMs = 2000; // 03번 설계서 1.5절 "통신" 규칙과 동일
constexpr int kTimeoutCheckIntervalMs = 500;

qint64 currentTimeMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}
} // namespace

RosSensorTelemetryProvider::RosSensorTelemetryProvider(QObject *parent)
    : SensorTelemetryProvider(parent)
    , m_timeoutTimer(new QTimer(this))
    , m_node(std::make_shared<rclcpp::Node>("medirun_gui_sensor_telemetry"))
    , m_executor(std::make_unique<rclcpp::executors::SingleThreadedExecutor>())
    , m_started(false)
    , m_connected(false)
    , m_lastMessageMs(0)
{
    m_timeoutTimer->setInterval(kTimeoutCheckIntervalMs);
    connect(m_timeoutTimer, &QTimer::timeout, this, &RosSensorTelemetryProvider::checkTimeout);

    // 로봇에서 실제로 뜨는 건 medirun_sensor가 아니라 레거시 arduino_bridge
    // 패키지의 serial_bridge 노드라, 토픽이 /medirun/sensor/*가 아니라
    // /arduino/*다 (src/arduino_bridge/src/serial_bridge.cpp 참고).
    m_frontDistanceSub = m_node->create_subscription<std_msgs::msg::Int32>(
        "/arduino/front_distance_cm", rclcpp::QoS(10),
        [this](const std_msgs::msg::Int32::SharedPtr msg) {
            m_lastMessageMs.store(currentTimeMs());
            if (!m_connected.exchange(true)) {
                emit connectionStateChanged(true);
            }
            emit frontDistanceChanged(msg->data);
        });

    m_rearDistanceSub = m_node->create_subscription<std_msgs::msg::Int32>(
        "/arduino/rear_distance_cm", rclcpp::QoS(10),
        [this](const std_msgs::msg::Int32::SharedPtr msg) {
            m_lastMessageMs.store(currentTimeMs());
            if (!m_connected.exchange(true)) {
                emit connectionStateChanged(true);
            }
            emit rearDistanceChanged(msg->data);
        });

    // serial_bridge가 이제 HALL: 값을 1초마다 재전송하므로(변화 없어도) 늦게
    // 구독해도 곧 최신 상태를 받는다 — transient_local 없이 front/rear distance와
    // 같은 기본(volatile) QoS로 맞춘다 (전에는 QoS 불일치/durability 문제로
    // 이 토픽만 아예 안 붙는 경우가 있었다).
    m_payloadLoadedSub = m_node->create_subscription<std_msgs::msg::Bool>(
        "/arduino/payload_loaded", rclcpp::QoS(10),
        [this](const std_msgs::msg::Bool::SharedPtr msg) {
            m_lastMessageMs.store(currentTimeMs());
            if (!m_connected.exchange(true)) {
                emit connectionStateChanged(true);
            }
            emit payloadLoadedChanged(msg->data);
        });
}

RosSensorTelemetryProvider::~RosSensorTelemetryProvider()
{
    stopExecutor();
}

void RosSensorTelemetryProvider::start()
{
    if (m_started.exchange(true)) {
        return;
    }

    m_executor->add_node(m_node);
    m_spinThread = std::thread([this]() { m_executor->spin(); });

    m_timeoutTimer->start();
}

void RosSensorTelemetryProvider::checkTimeout()
{
    if (m_lastMessageMs.load() == 0) {
        return;
    }

    const qint64 elapsedMs = currentTimeMs() - m_lastMessageMs.load();
    const bool stillConnected = elapsedMs < kMessageTimeoutMs;
    if (m_connected.exchange(stillConnected) != stillConnected) {
        emit connectionStateChanged(stillConnected);
    }
}

void RosSensorTelemetryProvider::stopExecutor()
{
    if (!m_started.exchange(false)) {
        return;
    }

    m_timeoutTimer->stop();

    if (m_executor != nullptr) {
        m_executor->cancel();
    }

    if (m_spinThread.joinable()) {
        m_spinThread.join();
    }

    if (m_executor != nullptr && m_node != nullptr) {
        m_executor->remove_node(m_node);
    }
}
