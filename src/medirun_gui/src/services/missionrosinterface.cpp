#include "missionrosinterface.h"

#include <rclcpp/rclcpp.hpp>

#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/string.hpp>
#include <robot_audio_interfaces/msg/audio_command.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QUuid>

#include <chrono>
#include <mutex>
#include <thread>

namespace {

constexpr qint64 kMinReasonableRequestedAtSec = 1577836800; // 2020-01-01 00:00:00 UTC

// 여러 medirun_gui 창이 같은 로봇에 동시 접속했을 때 딱 하나만 실제로 명령을
// 내리게 하는 창 간 자체 조율(리더 선출) 타이밍 — 시계 동기화에 기대지 않고
// instance_id 문자열 비교만으로 동점을 가른다(MissionRosNode 참고).
constexpr int kControlDiscoveryWindowMs = 1500;
constexpr int kControlHeartbeatIntervalMs = 1000;
constexpr int kControlLeaderTimeoutMs = 3500;

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

std::string makeArrivalNoticeJson(bool armed,
                                  const QString &missionId,
                                  const QString &queueId,
                                  double homeX,
                                  double homeY,
                                  double homeYaw,
                                  const QString &frameId,
                                  const QVector<NavPose> &trajectoryPoses)
{
    QJsonObject object;
    object.insert(QStringLiteral("armed"), armed);
    object.insert(QStringLiteral("mission_id"), missionId);
    object.insert(QStringLiteral("queue_id"), queueId);
    if (armed) {
        QJsonObject homePose;
        homePose.insert(QStringLiteral("x"), homeX);
        homePose.insert(QStringLiteral("y"), homeY);
        homePose.insert(QStringLiteral("yaw"), homeYaw);
        homePose.insert(QStringLiteral("frame_id"), frameId);
        object.insert(QStringLiteral("home_pose"), homePose);
        if (!trajectoryPoses.isEmpty()) {
            QJsonArray trajectory;
            for (const NavPose &pose : trajectoryPoses) {
                QJsonObject step;
                step.insert(QStringLiteral("x"), pose.x);
                step.insert(QStringLiteral("y"), pose.y);
                step.insert(QStringLiteral("yaw"), pose.yaw);
                trajectory.append(step);
            }
            object.insert(QStringLiteral("trajectory"), trajectory);
        }
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)).toStdString();
}

} // namespace

enum class ControlRole
{
    Discovering,
    Leader,
    Follower
};

class MissionRosNode : public rclcpp::Node
{
public:
    MissionRosNode(MissionRosInterface *bridge, QString instanceId)
        : rclcpp::Node("medirun_gui_mission_bridge")
        , m_bridge(bridge)
        , m_instanceId(std::move(instanceId))
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

        m_dockRequestPub = create_publisher<std_msgs::msg::Bool>("/medirun/vision/dock_request", 10);
        m_dockResultSub = create_subscription<std_msgs::msg::String>(
            "/medirun/vision/dock_result", 10,
            [this](const std_msgs::msg::String &msg) {
                const QJsonObject object = parseJsonObject(msg.data);
                const QString status = object.value(QStringLiteral("status")).toString();
                const QString reason = object.value(QStringLiteral("reason")).toString();
                emit m_bridge->dockResultReceived(status == QStringLiteral("SUCCEEDED"), reason);
            });

        m_arrivalNoticePub = create_publisher<std_msgs::msg::String>("/medirun/mission/arrival_notice", 10);
        m_piReturnStatusSub = create_subscription<std_msgs::msg::String>(
            "/medirun/mission/pi_return_status", 10,
            [this](const std_msgs::msg::String &msg) {
                const QJsonObject object = parseJsonObject(msg.data);
                emit m_bridge->piReturnStatusReceived(object.value(QStringLiteral("phase")).toString(),
                                                      object.value(QStringLiteral("mission_id")).toString(),
                                                      object.value(QStringLiteral("queue_id")).toString(),
                                                      object.value(QStringLiteral("waypoint_index")).toInt(-1),
                                                      object.value(QStringLiteral("poses_total")).toInt(-1),
                                                      object.value(QStringLiteral("replayed")).toBool(false));
            });

        m_missionSnapshotPub = create_publisher<std_msgs::msg::String>("/medirun/gui/mission_snapshot", 10);
        m_missionSnapshotSub = create_subscription<std_msgs::msg::String>(
            "/medirun/gui/mission_snapshot", 10,
            [this](const std_msgs::msg::String &msg) {
                emit m_bridge->missionSnapshotReceived(QString::fromStdString(msg.data));
            });

        m_controlHeartbeatPub = create_publisher<std_msgs::msg::String>("/medirun/gui/control_heartbeat", 10);
        m_controlHeartbeatSub = create_subscription<std_msgs::msg::String>(
            "/medirun/gui/control_heartbeat", 10,
            [this](const std_msgs::msg::String &msg) { handleControlHeartbeat(msg); });
        beginControlDiscovery();
        m_controlTickTimer = create_wall_timer(
            std::chrono::milliseconds(kControlHeartbeatIntervalMs),
            [this]() { controlTick(); });
    }

    // 로봇 조종 권한을 쥔 채로 창이 정상 종료될 때 호출 — "release" 신호를
    // 보내면 다른 창들이 kControlLeaderTimeoutMs까지 기다리지 않고 바로
    // 재탐색해서 넘겨받는다.
    void releaseControlIfLeader()
    {
        if (m_controlRole == ControlRole::Leader) {
            publishControlHeartbeat(QStringLiteral("release"));
        }
    }

    void requestDocking()
    {
        if (!m_dockRequestPub) {
            ensureInterfaces();
        }
        std_msgs::msg::Bool msg;
        msg.data = true;
        m_dockRequestPub->publish(msg);
        emit m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("[ROS2] dock_request published"));
    }

    void cancelDocking()
    {
        if (!m_dockRequestPub) {
            ensureInterfaces();
        }
        std_msgs::msg::Bool msg;
        msg.data = false;
        m_dockRequestPub->publish(msg);
        emit m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("[ROS2] dock_request(false) published — cancel"));
    }

    void publishMissionSnapshot(const QString &json)
    {
        if (!m_missionSnapshotPub) {
            ensureInterfaces();
        }
        std_msgs::msg::String msg;
        msg.data = json.toStdString();
        m_missionSnapshotPub->publish(msg);
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

    void publishArrivalNotice(bool armed,
                              const QString &missionId,
                              const QString &queueId,
                              double homeX,
                              double homeY,
                              double homeYaw,
                              const QString &frameId,
                              const QVector<NavPose> &trajectoryPoses)
    {
        if (!m_arrivalNoticePub) {
            ensureInterfaces();
        }
        std_msgs::msg::String msg;
        msg.data = makeArrivalNoticeJson(armed, missionId, queueId, homeX, homeY, homeYaw, frameId, trajectoryPoses);
        m_arrivalNoticePub->publish(msg);
        emit m_bridge->logEvent(QStringLiteral("INFO"),
                                QStringLiteral("[ROS2] arrival_notice published armed=%1 mission_id=%2").arg(armed).arg(missionId));
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
    // 조종 권한 리더 선출 — 자세한 알고리즘은 파일 상단 상수 설명 참고.
    // 시작 시 탐색 창(kControlDiscoveryWindowMs) 동안 다른 창의 heartbeat가
    // 보이면 그 창을 리더로 인정(Follower)하고, 안 보이면 스스로 리더가
    // 된다(Leader). 두 창이 동시에 떠서 서로를 못 보고 둘 다 리더를 자처하는
    // 드문 경우엔 instance_id가 사전순으로 더 작은 쪽이 이긴다 — 진 쪽은 다음
    // heartbeat 수신 즉시 물러난다.
    void handleControlHeartbeat(const std_msgs::msg::String &msg)
    {
        const QJsonObject object = parseJsonObject(msg.data);
        const QString senderId = object.value(QStringLiteral("instance_id")).toString();
        const QString action = object.value(QStringLiteral("action")).toString();
        if (senderId.isEmpty() || senderId == m_instanceId) {
            return;
        }
        const qint64 nowMs = steadyNowMs();

        if (action == QStringLiteral("release")) {
            if (m_controlRole == ControlRole::Follower && senderId == m_trackedLeaderId) {
                emit m_bridge->logEvent(QStringLiteral("INFO"),
                                        QStringLiteral("[CONTROL] 조종 중이던 창이 종료돼 재탐색합니다."));
                beginControlDiscovery();
            }
            return;
        }

        if (m_controlRole == ControlRole::Discovering) {
            if (m_trackedLeaderId.isEmpty() || senderId < m_trackedLeaderId) {
                m_trackedLeaderId = senderId;
            }
            m_lastLeaderSeenMs = nowMs;
            return;
        }

        if (m_controlRole == ControlRole::Leader) {
            if (senderId < m_instanceId) {
                becomeFollower(senderId, nowMs);
                emit m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("[CONTROL] 조종 권한을 다른 창에 양보했습니다."));
            }
            return;
        }

        // Follower
        if (senderId == m_trackedLeaderId) {
            m_lastLeaderSeenMs = nowMs;
        } else if (senderId < m_trackedLeaderId) {
            m_trackedLeaderId = senderId;
            m_lastLeaderSeenMs = nowMs;
        }
    }

    void beginControlDiscovery()
    {
        m_controlRole = ControlRole::Discovering;
        m_trackedLeaderId.clear();
        emit m_bridge->controlAuthorityChanged(false);
        if (m_controlDiscoveryTimer) {
            m_controlDiscoveryTimer->cancel();
        }
        m_controlDiscoveryTimer = create_wall_timer(
            std::chrono::milliseconds(kControlDiscoveryWindowMs),
            [this]() { finishControlDiscovery(); });
    }

    void finishControlDiscovery()
    {
        if (m_controlDiscoveryTimer) {
            m_controlDiscoveryTimer->cancel();
            m_controlDiscoveryTimer.reset();
        }
        if (m_controlRole != ControlRole::Discovering) {
            return; // 탐색 중 이미 다른 경로로 역할이 정해짐
        }
        if (!m_trackedLeaderId.isEmpty()) {
            becomeFollower(m_trackedLeaderId, steadyNowMs());
        } else {
            becomeLeader();
        }
    }

    void becomeLeader()
    {
        m_controlRole = ControlRole::Leader;
        m_trackedLeaderId.clear();
        publishControlHeartbeat(QStringLiteral("heartbeat"));
        emit m_bridge->controlAuthorityChanged(true);
        emit m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("[CONTROL] 이 창이 로봇 조종 권한을 획득했습니다."));
    }

    void becomeFollower(const QString &leaderId, qint64 nowMs)
    {
        m_controlRole = ControlRole::Follower;
        m_trackedLeaderId = leaderId;
        m_lastLeaderSeenMs = nowMs;
        emit m_bridge->controlAuthorityChanged(false);
        emit m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("[CONTROL] 다른 창이 이미 로봇을 조종 중입니다 — 읽기 전용."));
    }

    void controlTick()
    {
        if (m_controlRole == ControlRole::Leader) {
            publishControlHeartbeat(QStringLiteral("heartbeat"));
            return;
        }
        if (m_controlRole == ControlRole::Follower
            && steadyNowMs() - m_lastLeaderSeenMs > kControlLeaderTimeoutMs) {
            emit m_bridge->logEvent(QStringLiteral("WARN"),
                                    QStringLiteral("[CONTROL] 조종 권한을 가진 창의 응답이 끊겨 재탐색합니다."));
            beginControlDiscovery();
        }
    }

    void publishControlHeartbeat(const QString &action)
    {
        if (!m_controlHeartbeatPub) {
            return;
        }
        QJsonObject object;
        object.insert(QStringLiteral("instance_id"), m_instanceId);
        object.insert(QStringLiteral("action"), action);
        std_msgs::msg::String msg;
        msg.data = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)).toStdString();
        m_controlHeartbeatPub->publish(msg);
    }

    static qint64 steadyNowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    MissionRosInterface *m_bridge;
    QString m_instanceId;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_dispatchSub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_missionStateSub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_navigationStatusSub;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr m_uiMissionPub;
    rclcpp::Publisher<robot_audio_interfaces::msg::AudioCommand>::SharedPtr m_audioCommandPub;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr m_dockRequestPub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_dockResultSub;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr m_arrivalNoticePub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_piReturnStatusSub;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr m_missionSnapshotPub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_missionSnapshotSub;
    ControlRole m_controlRole {ControlRole::Discovering};
    QString m_trackedLeaderId;
    qint64 m_lastLeaderSeenMs {0};
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr m_controlHeartbeatPub;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr m_controlHeartbeatSub;
    rclcpp::TimerBase::SharedPtr m_controlDiscoveryTimer;
    rclcpp::TimerBase::SharedPtr m_controlTickTimer;
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

    const QString instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_impl = std::make_unique<Impl>();
    m_impl->node = std::make_shared<MissionRosNode>(this, instanceId);
    m_impl->executor.add_node(m_impl->node);
    m_impl->spinThread = std::thread([this]() { m_impl->executor.spin(); });
}

MissionRosInterface::~MissionRosInterface()
{
    m_impl->node->releaseControlIfLeader();
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

void MissionRosInterface::publishArrivalNotice(bool armed,
                                               const QString &missionId,
                                               const QString &queueId,
                                               double homeX,
                                               double homeY,
                                               double homeYaw,
                                               const QString &frameId,
                                               const QVector<NavPose> &trajectoryPoses)
{
    m_impl->node->publishArrivalNotice(armed, missionId, queueId, homeX, homeY, homeYaw, frameId, trajectoryPoses);
}

void MissionRosInterface::requestDocking()
{
    m_impl->node->requestDocking();
}

void MissionRosInterface::cancelDocking()
{
    m_impl->node->cancelDocking();
}

void MissionRosInterface::publishMissionSnapshot(const QString &json)
{
    m_impl->node->publishMissionSnapshot(json);
}
