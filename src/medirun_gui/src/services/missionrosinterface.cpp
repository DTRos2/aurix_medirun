#include "missionrosinterface.h"

#include <rclcpp/rclcpp.hpp>

#include <std_msgs/msg/string.hpp>
#include <robot_audio_interfaces/msg/audio_command.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>

#include <mutex>
#include <thread>

namespace {

constexpr qint64 kMinReasonableRequestedAtSec = 1577836800; // 2020-01-01 00:00:00 UTC

QDateTime parseRequestedAt(const QJsonObject &object)
{
    const double requestedAtSec = object.value(QStringLiteral("requested_at_sec")).toDouble(-1.0);
    if (requestedAtSec > 0.0) {
        const qint64 requestedAtWholeSec = static_cast<qint64>(requestedAtSec);
        const qint64 nowSec = QDateTime::currentSecsSinceEpoch();
        // 일부 발행자가 epoch seconds 대신 "당일 경과 초" 같은 작은 숫자를 보내는
        // 경우가 있어, 1970년대로 저장되지 않도록 비정상 범위는 현재 시각으로
        // 보정한다. 미래값도 과도하면 같이 막는다.
        if (requestedAtWholeSec >= kMinReasonableRequestedAtSec && requestedAtWholeSec <= nowSec + 24 * 60 * 60) {
            return QDateTime::fromSecsSinceEpoch(requestedAtWholeSec);
        }
        RCLCPP_WARN(rclcpp::get_logger("medirun_gui_mission_bridge"),
                    "requested_at_sec out of range: %.3f, fallback to current time", requestedAtSec);
    }
    return QDateTime::currentDateTime();
}

QJsonObject parseJsonObject(const std::string &text)
{
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(text));
    return doc.object();
}

std::string makeCommandJson(const QString &command,
                            const QString &missionId,
                            const QString &queueId,
                            const QString &waypointId)
{
    QJsonObject object;
    object.insert(QStringLiteral("command"), command);
    object.insert(QStringLiteral("mission_id"), missionId);
    object.insert(QStringLiteral("queue_id"), queueId);
    object.insert(QStringLiteral("waypoint_id"), waypointId);
    object.insert(QStringLiteral("requested_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)).toStdString();
}

} // namespace

class MissionRosNode : public rclcpp::Node
{
public:
    explicit MissionRosNode(MissionRosInterface *bridge)
        : rclcpp::Node("medirun_gui_mission_bridge")
        , m_bridge(bridge)
    {
        ensureInterfaces();
    }

    void ensureInterfaces()
    {
        // retryCommunication()이 "재시도" 버튼을 누를 때마다 이 함수를 무조건
        // 다시 호출하는데, 예전엔 매번 create_subscription()을 새로 불러
        // m_dispatchSub를 덮어썼다. 이 노드는 별도 스레드에서 executor가 계속
        // spin 중이라, 기존 구독이 실제로 정리되기 전에 새 구독이 등록되면
        // 같은 /medirun/mission/dispatch_request 메시지 하나에 콜백이 두 번
        // 걸려 호출 버튼 하나로 미션이 두 개 시작되는 문제가 있었다(실측:
        // 대기 큐 대신 서로를 밀어내며 즉시 실패). 한 번 구독을 만든 뒤로는
        // 다시 만들지 않고 그대로 재사용한다 — DDS 구독은 발행자가 재시작돼도
        // 자동으로 재발견되므로 재시도 시 다시 만들 필요가 없다.
        if (m_dispatchSub) {
            return;
        }
        m_dispatchSub = create_subscription<std_msgs::msg::String>(
            "/medirun/mission/dispatch_request", 10,
            [this](const std_msgs::msg::String &msg) {
                const QJsonObject object = parseJsonObject(msg.data);
                emit m_bridge->dispatchRequested(object.value(QStringLiteral("button_id")).toString(),
                                                 object.value(QStringLiteral("request_source")).toString(),
                                                 object.value(QStringLiteral("mission_id")).toString(),
                                                 parseRequestedAt(object));
                emit m_bridge->missionHeartbeat(QStringLiteral("dispatch_request"), QDateTime::currentDateTime());
            });

        m_missionStateSub = create_subscription<std_msgs::msg::String>(
            "/medirun/mission/state", 10,
            [this](const std_msgs::msg::String &) {
                emit m_bridge->missionHeartbeat(QStringLiteral("mission_state"), QDateTime::currentDateTime());
            });

        m_navigationStatusSub = create_subscription<std_msgs::msg::String>(
            "/medirun/navigation/status", 10,
            [this](const std_msgs::msg::String &) {
                emit m_bridge->missionHeartbeat(QStringLiteral("navigation_status"), QDateTime::currentDateTime());
            });

        m_uiMissionPub = create_publisher<std_msgs::msg::String>("/medirun/ui/mission_command", 10);
        // Pi의 robot_audio_output(audio_output_node)이 구독하는 토픽 — 이
        // 저장소가 아니라 Pi의 별도 워크스페이스(~/turtlebot3_ws)에서 도는
        // 노드다(2026-08-19 실기 확인). 인터페이스만 robot_audio_interfaces로
        // 이 저장소에 가져와 빌드한다.
        m_audioCommandPub = create_publisher<robot_audio_interfaces::msg::AudioCommand>("/audio/command", 10);
    }

    void publishMissionCommand(const QString &command,
                               const QString &missionId,
                               const QString &queueId,
                               const QString &waypointId)
    {
        if (!m_uiMissionPub) {
            ensureInterfaces();
        }
        std_msgs::msg::String msg;
        msg.data = makeCommandJson(command, missionId, queueId, waypointId);
        m_uiMissionPub->publish(msg);
        emit m_bridge->logEvent(QStringLiteral("INFO"),
                                QStringLiteral("[ROS2] mission command published command=%1 mission_id=%2 queue_id=%3 waypoint_id=%4")
                                    .arg(command, missionId, queueId, waypointId));
    }

    void publishAudioAnnouncement(const QString &text)
    {
        if (!m_audioCommandPub) {
            ensureInterfaces();
        }
        robot_audio_interfaces::msg::AudioCommand msg;
        msg.type = robot_audio_interfaces::msg::AudioCommand::TYPE_TTS;
        msg.text = text.toStdString();
        msg.volume = 1.0F;
        msg.repeat = 1;
        m_audioCommandPub->publish(msg);
        emit m_bridge->logEvent(QStringLiteral("INFO"),
                                QStringLiteral("[AUDIO] announcement published text=\"%1\"").arg(text));
    }

    void retryCommunication()
    {
        ensureInterfaces();
        const auto topics = get_topic_names_and_types();
        const bool dispatchTopicPresent = topics.find("/medirun/mission/dispatch_request") != topics.end();
        const bool missionStatePresent = topics.find("/medirun/mission/state") != topics.end();
        const bool navigationStatusPresent = topics.find("/medirun/navigation/status") != topics.end();
        const bool success = dispatchTopicPresent && missionStatePresent && navigationStatusPresent;
        const QString detail = QStringLiteral("dispatch=%1 mission_state=%2 navigation_status=%3")
                                   .arg(dispatchTopicPresent ? QStringLiteral("ok") : QStringLiteral("missing"))
                                   .arg(missionStatePresent ? QStringLiteral("ok") : QStringLiteral("missing"))
                                   .arg(navigationStatusPresent ? QStringLiteral("ok") : QStringLiteral("missing"));
        emit m_bridge->communicationRetryFinished(success, detail);
    }

private:
    MissionRosInterface *m_bridge;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_dispatchSub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_missionStateSub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_navigationStatusSub;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr m_uiMissionPub;
    rclcpp::Publisher<robot_audio_interfaces::msg::AudioCommand>::SharedPtr m_audioCommandPub;
};

struct MissionRosInterface::Impl
{
    std::shared_ptr<MissionRosNode> node;
    rclcpp::executors::SingleThreadedExecutor executor;
    std::thread spinThread;
};

MissionRosInterface::MissionRosInterface(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QDateTime>("QDateTime");
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        if (!rclcpp::ok()) {
            int argc = 0;
            rclcpp::init(argc, nullptr);
        }
    });

    m_impl = std::make_unique<Impl>();
    m_impl->node = std::make_shared<MissionRosNode>(this);
    m_impl->executor.add_node(m_impl->node);
    m_impl->spinThread = std::thread([this]() { m_impl->executor.spin(); });
}

MissionRosInterface::~MissionRosInterface()
{
    m_impl->executor.cancel();
    if (m_impl->spinThread.joinable()) {
        m_impl->spinThread.join();
    }
    m_impl->executor.remove_node(m_impl->node);
    m_impl->node.reset();
}

void MissionRosInterface::publishMissionCommand(const QString &command,
                                                const QString &missionId,
                                                const QString &queueId,
                                                const QString &waypointId)
{
    m_impl->node->publishMissionCommand(command, missionId, queueId, waypointId);
}

void MissionRosInterface::publishAudioAnnouncement(const QString &text)
{
    m_impl->node->publishAudioAnnouncement(text);
}

void MissionRosInterface::retryCommunication()
{
    m_impl->node->retryCommunication();
}
