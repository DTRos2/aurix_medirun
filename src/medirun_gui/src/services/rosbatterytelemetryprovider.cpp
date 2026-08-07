#include "rosbatterytelemetryprovider.h"

#include <QDateTime>
#include <QSqlDatabase>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kBatteryTimeoutMs = 5000;
constexpr int kTimeoutCheckIntervalMs = 1000;
constexpr double kBatteryPercentTauSec = 5.0;
constexpr double kBatteryPercentHysteresis = 0.75;

qint64 currentTimeMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

RosBatteryTelemetryProvider::RosBatteryTelemetryProvider(QObject *parent)
    : TelemetryProvider(parent)
    , m_timeoutTimer(new QTimer(this))
    , m_node(std::make_shared<rclcpp::Node>("medirun_gui_battery_monitor"))
    , m_subscription()
    , m_executor(std::make_unique<rclcpp::executors::SingleThreadedExecutor>())
    , m_started(false)
    , m_connectionLost(false)
    , m_hasReceivedMessage(false)
    , m_lastMessageMs(0)
    , m_filteredBatteryPercent(0.0)
    , m_filteredBatteryPercentTimeMs(0)
    , m_displayedBatteryPercent(0)
    , m_hasFilteredBatteryPercent(false)
    , m_hasDisplayedBatteryPercent(false)
    , m_dbConnected(false)
{
    m_timeoutTimer->setInterval(kTimeoutCheckIntervalMs);
    connect(m_timeoutTimer, &QTimer::timeout, this, &RosBatteryTelemetryProvider::checkBatteryTimeout);

    m_subscription = m_node->create_subscription<sensor_msgs::msg::BatteryState>(
        "/battery_state",
        rclcpp::SystemDefaultsQoS(),
        [this](const sensor_msgs::msg::BatteryState::SharedPtr msg) {
            handleBatteryState(msg);
        });
}

RosBatteryTelemetryProvider::~RosBatteryTelemetryProvider()
{
    stopExecutor();
}

void RosBatteryTelemetryProvider::start()
{
    if (m_started.exchange(true)) {
        return;
    }

    refreshDbState();
    emitSnapshot(buildWaitingSnapshot());

    m_executor->add_node(m_node);
    m_spinThread = std::thread([this]() {
        m_executor->spin();
    });

    m_timeoutTimer->start();
}

void RosBatteryTelemetryProvider::checkBatteryTimeout()
{
    // DB 연결 상태는 배터리 토픽 수신과 무관하게 바뀔 수 있으므로(배터리
    // 토픽이 아예 안 들어오는 환경에서도) 매 틱마다 별도로 확인해 헤더의
    // DB 표시가 갱신되도록 한다. 이 타이머는 GUI 스레드에서 돌기 때문에
    // 여기서만 QSqlDatabase를 조회한다(다른 스레드에서 조회하면 안 됨).
    const bool wasConnected = m_dbConnected.load();
    refreshDbState();
    if (m_dbConnected.load() != wasConnected) {
        TelemetrySnapshot snapshot = m_lastSnapshot;
        applyDbState(snapshot);
        emitSnapshot(snapshot);
    }

    if (!m_hasReceivedMessage.load()) {
        return;
    }

    const qint64 elapsedMs = currentTimeMs() - m_lastMessageMs.load();
    if (elapsedMs < kBatteryTimeoutMs) {
        return;
    }

    if (m_connectionLost.exchange(true)) {
        return;
    }

    emitSnapshot(buildDisconnectedSnapshot());
}

void RosBatteryTelemetryProvider::stopExecutor()
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

void RosBatteryTelemetryProvider::handleBatteryState(const sensor_msgs::msg::BatteryState::SharedPtr msg)
{
    m_hasReceivedMessage.store(true);
    m_connectionLost.store(false);
    m_lastMessageMs.store(currentTimeMs());

    const double rawBatteryPercent = normalizeBatteryPercent(msg->percentage);
    const double filteredBatteryPercent = filterBatteryPercent(rawBatteryPercent);
    const int displayedBatteryPercent = stabilizeDisplayedPercent(filteredBatteryPercent);

    emitSnapshot(buildBatterySnapshot(displayedBatteryPercent, msg->voltage));
}

TelemetrySnapshot RosBatteryTelemetryProvider::buildWaitingSnapshot() const
{
    TelemetrySnapshot snapshot;
    snapshot.batteryPercentText = QStringLiteral("--%");
    snapshot.rosText = QStringLiteral("수신 대기");
    snapshot.rosState = ConnectionState::Warning;
    snapshot.batteryMessageReceived = false;
    applyDbState(snapshot);
    return snapshot;
}

TelemetrySnapshot RosBatteryTelemetryProvider::buildDisconnectedSnapshot() const
{
    TelemetrySnapshot snapshot;
    snapshot.batteryPercentText = QStringLiteral("--%");
    snapshot.rosText = QStringLiteral("연결 끊김");
    snapshot.rosState = ConnectionState::Disconnected;
    snapshot.batteryMessageReceived = false;
    applyDbState(snapshot);
    return snapshot;
}

TelemetrySnapshot RosBatteryTelemetryProvider::buildBatterySnapshot(int batteryPercent, double voltage) const
{
    TelemetrySnapshot snapshot;
    snapshot.batteryPercent = batteryPercent;
    snapshot.batteryPercentText = QStringLiteral("%1%").arg(batteryPercent);
    snapshot.batteryVoltage = voltage;
    snapshot.batteryMessageReceived = true;
    snapshot.rosText = QStringLiteral("연결됨");
    snapshot.rosState = ConnectionState::Connected;
    applyDbState(snapshot);
    return snapshot;
}

void RosBatteryTelemetryProvider::refreshDbState()
{
    QSqlDatabase db = QSqlDatabase::database(QSqlDatabase::defaultConnection, false);
    m_dbConnected.store(db.isValid() && db.isOpen());
}

void RosBatteryTelemetryProvider::applyDbState(TelemetrySnapshot &snapshot) const
{
    const bool connected = m_dbConnected.load();
    snapshot.dbState = connected ? ConnectionState::Connected : ConnectionState::Disconnected;
    snapshot.dbText = connected ? QStringLiteral("연결됨") : QStringLiteral("연결 끊김");
}

void RosBatteryTelemetryProvider::emitSnapshot(TelemetrySnapshot snapshot)
{
    m_lastSnapshot = snapshot;
    emit snapshotUpdated(snapshot);
}

double RosBatteryTelemetryProvider::normalizeBatteryPercent(double percentage) const
{
    double normalized = percentage;
    if (percentage >= 0.0 && percentage <= 1.0) {
        normalized = percentage * 100.0;
    } else if (percentage < 0.0) {
        normalized = 0.0;
    }

    return std::clamp(normalized, 0.0, 100.0);
}

double RosBatteryTelemetryProvider::filterBatteryPercent(double batteryPercent)
{
    std::lock_guard<std::mutex> lock(m_batteryStateMutex);

    const qint64 nowMs = currentTimeMs();
    if (!m_hasFilteredBatteryPercent) {
        m_filteredBatteryPercent = batteryPercent;
        m_filteredBatteryPercentTimeMs = nowMs;
        m_hasFilteredBatteryPercent = true;
        return m_filteredBatteryPercent;
    }

    const double dtSec = std::max(0.0, (nowMs - m_filteredBatteryPercentTimeMs) / 1000.0);
    const double alpha = 1.0 - std::exp(-dtSec / kBatteryPercentTauSec);
    m_filteredBatteryPercent += alpha * (batteryPercent - m_filteredBatteryPercent);
    m_filteredBatteryPercentTimeMs = nowMs;
    return m_filteredBatteryPercent;
}

int RosBatteryTelemetryProvider::stabilizeDisplayedPercent(double filteredPercent)
{
    std::lock_guard<std::mutex> lock(m_batteryStateMutex);

    if (!m_hasDisplayedBatteryPercent
        || std::abs(filteredPercent - static_cast<double>(m_displayedBatteryPercent))
            >= (0.5 + kBatteryPercentHysteresis)) {
        m_displayedBatteryPercent = std::clamp(static_cast<int>(std::lround(filteredPercent)), 0, 100);
        m_hasDisplayedBatteryPercent = true;
    }

    return m_displayedBatteryPercent;
}
