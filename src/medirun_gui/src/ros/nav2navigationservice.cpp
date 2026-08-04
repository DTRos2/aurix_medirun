#include "nav2navigationservice.h"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <lifecycle_msgs/msg/state.hpp>
#include <lifecycle_msgs/srv/get_state.hpp>
#include <nav2_msgs/action/follow_waypoints.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav2_msgs/msg/speed_limit.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <tf2/exceptions.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <cmath>
#include <mutex>
#include <thread>
#include <utility>

namespace {

double yawFromQuaternion(const geometry_msgs::msg::Quaternion &q)
{
    const double sinyCosp = 2.0 * (q.w * q.z + q.x * q.y);
    const double cosyCosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    return std::atan2(sinyCosp, cosyCosp);
}

void quaternionFromYaw(double yaw, double &z, double &w)
{
    z = std::sin(yaw / 2.0);
    w = std::cos(yaw / 2.0);
}

geometry_msgs::msg::PoseStamped makePoseStamped(double x, double y, double yaw,
                                                 const std::string &frameId,
                                                 const rclcpp::Time &stamp)
{
    geometry_msgs::msg::PoseStamped ps;
    ps.header.frame_id = frameId;
    ps.header.stamp = stamp;
    ps.pose.position.x = x;
    ps.pose.position.y = y;
    quaternionFromYaw(yaw, ps.pose.orientation.z, ps.pose.orientation.w);
    return ps;
}

QString resultCodeText(rclcpp_action::ResultCode code)
{
    switch (code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
        return QStringLiteral("성공");
    case rclcpp_action::ResultCode::CANCELED:
        return QStringLiteral("취소됨");
    case rclcpp_action::ResultCode::ABORTED:
        return QStringLiteral("실패(aborted)");
    default:
        return QStringLiteral("종료(status=%1)").arg(static_cast<int>(code));
    }
}

} // namespace

// Plain rclcpp::Node (no Q_OBJECT/moc needed) that owns the action clients
// and calls back into the Nav2NavigationService bridge's (public) signal
// methods. Runs entirely inside Nav2NavigationService::Impl's spin thread.
class TurtlebotNavNode : public rclcpp::Node
{
public:
    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using FollowWaypoints = nav2_msgs::action::FollowWaypoints;
    using NavClient = rclcpp_action::Client<NavigateToPose>;
    using FollowClient = rclcpp_action::Client<FollowWaypoints>;

    explicit TurtlebotNavNode(Nav2NavigationService *bridge)
        : rclcpp::Node("medirun_gui_nav_bridge")
        , m_bridge(bridge)
    {
        m_navClient = rclcpp_action::create_client<NavigateToPose>(this, "navigate_to_pose");
        m_followClient = rclcpp_action::create_client<FollowWaypoints>(this, "follow_waypoints");
        m_initialPosePub = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>("initialpose", 10);
        m_speedLimitPub = create_publisher<nav2_msgs::msg::SpeedLimit>("speed_limit", 10);

        m_amclSub = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
            "amcl_pose", rclcpp::QoS(10),
            [this](const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
                const double yaw = yawFromQuaternion(msg->pose.pose.orientation);
                m_bridge->amclPoseUpdated(msg->pose.pose.position.x, msg->pose.pose.position.y, yaw);
            });

        m_tfBuffer = std::make_unique<tf2_ros::Buffer>(get_clock());
        m_tfListener = std::make_shared<tf2_ros::TransformListener>(*m_tfBuffer);

        m_scanSub = create_subscription<sensor_msgs::msg::LaserScan>(
            "scan", rclcpp::SensorDataQoS(),
            [this](const sensor_msgs::msg::LaserScan::SharedPtr msg) { handleScan(msg); });

        // bt_navigator/waypoint_follower의 실제 lifecycle 상태(active인지)를 직접
        // 물어본다. action_server_is_ready()는 액션 서버 "존재 여부"만 보는데,
        // nav2 lifecycle 노드는 on_configure() 단계에서 이미 액션 서버를 만들어
        // 두기 때문에 activate되기 훨씬 전(예: 아직 costmap이 map 프레임을
        // 기다리는 중)부터 true가 나와 버린다 — 실측: bt_navigator가 inactive인
        // 상태에서도 action_server_is_ready()==true였음. GetState 서비스로
        // PRIMARY_STATE_ACTIVE를 직접 확인해야 "goal을 실제로 받아줄 수 있는
        // 상태"를 정확히 판정할 수 있다.
        m_btNavigatorStateClient = create_client<lifecycle_msgs::srv::GetState>("/bt_navigator/get_state");
        m_waypointFollowerStateClient = create_client<lifecycle_msgs::srv::GetState>("/waypoint_follower/get_state");
        m_connectionCheckTimer = create_wall_timer(
            std::chrono::milliseconds(1000), [this]() { checkNav2Connection(); });
        checkNav2Connection();
    }

    void handleScan(const sensor_msgs::msg::LaserScan::SharedPtr &msg)
    {
        geometry_msgs::msg::TransformStamped laserToMap;
        try {
            // "map" 프레임이 아직 없으면(초기 위치 미설정) 예외가 나며, 이 경우
            // 조용히 스킵한다 — 초기 위치를 잡는 순간부터 자연스럽게 그려지기
            // 시작하면 된다. tf2::TimePointZero로 최신 가용 변환을 쓴다(스캔
            // 타임스탬프 엄격 매칭 시 타이밍 오차로 extrapolation 예외가 잦음).
            laserToMap = m_tfBuffer->lookupTransform("map", msg->header.frame_id, tf2::TimePointZero);
        } catch (const tf2::TransformException &) {
            return;
        }

        const double tx = laserToMap.transform.translation.x;
        const double ty = laserToMap.transform.translation.y;
        const double yaw = yawFromQuaternion(laserToMap.transform.rotation);
        const double cosYaw = std::cos(yaw);
        const double sinYaw = std::sin(yaw);

        QVector<QPointF> points;
        points.reserve(static_cast<int>(msg->ranges.size()));
        double angle = msg->angle_min;
        for (const float range : msg->ranges) {
            if (std::isfinite(range) && range >= msg->range_min && range <= msg->range_max) {
                const double lx = range * std::cos(angle);
                const double ly = range * std::sin(angle);
                points.append(QPointF(tx + lx * cosYaw - ly * sinYaw, ty + lx * sinYaw + ly * cosYaw));
            }
            angle += msg->angle_increment;
        }

        m_bridge->scanPointsUpdated(points);
    }

    void navigateToPose(double x, double y, double yaw, const QString &frameId)
    {
        if (!m_navClient->wait_for_action_server(std::chrono::seconds(1))) {
            m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("navigate_to_pose 액션 서버를 찾을 수 없습니다."));
            return;
        }

        NavigateToPose::Goal goal;
        goal.pose = makePoseStamped(x, y, yaw, frameId.toStdString(), now());

        m_bridge->logEvent(QStringLiteral("INFO"),
                            QStringLiteral("단일 목적지 주행 요청: (%1, %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));
        m_bridge->statusChanged(QStringLiteral("목적지 (%1, %2)로 이동 중...").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));

        NavClient::SendGoalOptions options;
        options.feedback_callback =
            [this](NavClient::GoalHandle::SharedPtr, const std::shared_ptr<const NavigateToPose::Feedback> feedback) {
                m_bridge->feedbackDistanceRemaining(static_cast<double>(feedback->distance_remaining));
            };
        options.goal_response_callback = [this](NavClient::GoalHandle::SharedPtr handle) {
            if (!handle) {
                m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("목적지 요청이 거부되었습니다."));
                m_bridge->statusChanged(QStringLiteral("대기 중"));
                return;
            }
            m_navGoalHandle = handle;
        };
        options.result_callback = [this](const NavClient::GoalHandle::WrappedResult &result) {
            m_navGoalHandle.reset();
            const QString text = resultCodeText(result.code);
            const QString level = (result.code == rclcpp_action::ResultCode::SUCCEEDED)
                                       ? QStringLiteral("INFO")
                                       : QStringLiteral("WARN");
            m_bridge->logEvent(level, QStringLiteral("단일 목적지 주행 결과: %1").arg(text));
            m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: %1)").arg(text));
        };

        m_navClient->async_send_goal(goal, options);
    }

    void followWaypoints(const QVector<NavPose> &poses, const QString &frameId)
    {
        if (!m_followClient->wait_for_action_server(std::chrono::seconds(1))) {
            m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("follow_waypoints 액션 서버를 찾을 수 없습니다."));
            return;
        }

        const rclcpp::Time stamp = now();
        FollowWaypoints::Goal goal;
        goal.poses.reserve(static_cast<size_t>(poses.size()));
        for (const NavPose &pose : poses) {
            goal.poses.push_back(makePoseStamped(pose.x, pose.y, pose.yaw, frameId.toStdString(), stamp));
        }
        m_followTotal = static_cast<int>(goal.poses.size());

        m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("경로 주행 요청: waypoint %1개").arg(m_followTotal));
        m_bridge->waypointProgress(0, m_followTotal);
        m_bridge->statusChanged(QStringLiteral("경로 주행 중 (0/%1)").arg(m_followTotal));

        FollowClient::SendGoalOptions options;
        options.feedback_callback =
            [this](FollowClient::GoalHandle::SharedPtr, const std::shared_ptr<const FollowWaypoints::Feedback> feedback) {
                m_bridge->waypointProgress(static_cast<int>(feedback->current_waypoint), m_followTotal);
            };
        options.goal_response_callback = [this](FollowClient::GoalHandle::SharedPtr handle) {
            if (!handle) {
                m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("경로 주행 요청이 거부되었습니다."));
                m_bridge->statusChanged(QStringLiteral("대기 중"));
                return;
            }
            m_followGoalHandle = handle;
        };
        options.result_callback = [this](const FollowClient::GoalHandle::WrappedResult &result) {
            m_followGoalHandle.reset();
            const bool missedAny = result.result && !result.result->missed_waypoints.empty();
            if (result.code == rclcpp_action::ResultCode::SUCCEEDED && !missedAny) {
                m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("경로 주행 완료"));
                m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: 성공)"));
            } else {
                const QString text = resultCodeText(result.code);
                const QString detail = missedAny
                                            ? QStringLiteral(" (놓친 waypoint %1개)").arg(result.result->missed_waypoints.size())
                                            : QString();
                m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("경로 주행 결과: %1%2").arg(text, detail));
                m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: %1)").arg(text));
            }
        };

        m_followClient->async_send_goal(goal, options);
    }

    void cancelGoal()
    {
        if (m_navGoalHandle) {
            m_navClient->async_cancel_goal(m_navGoalHandle);
            m_navGoalHandle.reset();
            m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("주행 취소를 요청했습니다."));
            m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
            return;
        }
        if (m_followGoalHandle) {
            m_followClient->async_cancel_goal(m_followGoalHandle);
            m_followGoalHandle.reset();
            m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("주행 취소를 요청했습니다."));
            m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
            return;
        }
        m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("취소할 진행 중인 주행이 없습니다."));
    }

    void publishInitialPose(double x, double y, double yaw, const QString &frameId)
    {
        geometry_msgs::msg::PoseWithCovarianceStamped msg;
        msg.header.frame_id = frameId.toStdString();
        msg.header.stamp = now();
        msg.pose.pose.position.x = x;
        msg.pose.pose.position.y = y;
        quaternionFromYaw(yaw, msg.pose.pose.orientation.z, msg.pose.pose.orientation.w);
        // RViz의 "2D Pose Estimate" 기본 공분산과 동일한 값 사용
        msg.pose.covariance[0] = 0.25;
        msg.pose.covariance[7] = 0.25;
        msg.pose.covariance[35] = 0.06853891945200942;

        m_initialPosePub->publish(msg);
        m_bridge->logEvent(QStringLiteral("INFO"),
                            QStringLiteral("초기 위치를 설정했습니다: x=%1, y=%2, yaw=%3")
                                .arg(x, 0, 'f', 2)
                                .arg(y, 0, 'f', 2)
                                .arg(yaw, 0, 'f', 2));
    }

    void setSpeedLimit(int percent)
    {
        nav2_msgs::msg::SpeedLimit msg;
        msg.header.stamp = now();
        msg.percentage = true;
        msg.speed_limit = static_cast<double>(percent);

        m_speedLimitPub->publish(msg);
        m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("주행 속도를 %1%로 설정했습니다.").arg(percent));
    }

    // bt_navigator/waypoint_follower 둘 다 GetState 서비스가 응답 가능하고
    // active여야 "연결됨"으로 본다. 두 서비스 응답이 비동기로 따로 도착하므로
    // 라운드마다(m_connectionCheckRound) 결과를 모아서 마지막에 한 번만 판정한다
    // — 서로 다른 라운드의 응답이 섞이면 오판정할 수 있어 라운드 번호로 구분한다.
    void checkNav2Connection()
    {
        if (!m_btNavigatorStateClient->service_is_ready() || !m_waypointFollowerStateClient->service_is_ready()) {
            updateNav2Connected(false);
            return;
        }

        const uint64_t round = ++m_connectionCheckRound;
        auto request = std::make_shared<lifecycle_msgs::srv::GetState::Request>();

        m_btNavigatorStateClient->async_send_request(
            request,
            [this, round](rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedFuture future) {
                handleLifecycleStateResult(round, m_btNavigatorActiveForRound,
                                            future.get()->current_state.id);
            });
        m_waypointFollowerStateClient->async_send_request(
            request,
            [this, round](rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedFuture future) {
                handleLifecycleStateResult(round, m_waypointFollowerActiveForRound,
                                            future.get()->current_state.id);
            });
    }

    void handleLifecycleStateResult(uint64_t round, std::pair<uint64_t, bool> &slot, uint8_t stateId)
    {
        slot = {round, stateId == lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE};
        if (m_btNavigatorActiveForRound.first != round || m_waypointFollowerActiveForRound.first != round) {
            return;
        }
        updateNav2Connected(m_btNavigatorActiveForRound.second && m_waypointFollowerActiveForRound.second);
    }

    void updateNav2Connected(bool connected)
    {
        if (connected == m_nav2Connected) {
            return;
        }
        m_nav2Connected = connected;
        m_bridge->nav2ConnectionChanged(connected);
    }

private:
    Nav2NavigationService *m_bridge;
    NavClient::SharedPtr m_navClient;
    FollowClient::SharedPtr m_followClient;
    rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr m_initialPosePub;
    rclcpp::Publisher<nav2_msgs::msg::SpeedLimit>::SharedPtr m_speedLimitPub;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr m_amclSub;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr m_scanSub;
    std::unique_ptr<tf2_ros::Buffer> m_tfBuffer;
    std::shared_ptr<tf2_ros::TransformListener> m_tfListener;
    NavClient::GoalHandle::SharedPtr m_navGoalHandle;
    FollowClient::GoalHandle::SharedPtr m_followGoalHandle;
    int m_followTotal {0};
    rclcpp::TimerBase::SharedPtr m_connectionCheckTimer;
    rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedPtr m_btNavigatorStateClient;
    rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedPtr m_waypointFollowerStateClient;
    uint64_t m_connectionCheckRound {0};
    // {round, active} — 그 라운드에서 마지막으로 받은 응답. round가 현재
    // m_connectionCheckRound와 다르면 아직 이번 라운드 응답이 안 온 것.
    std::pair<uint64_t, bool> m_btNavigatorActiveForRound {0, false};
    std::pair<uint64_t, bool> m_waypointFollowerActiveForRound {0, false};
    bool m_nav2Connected {false};
};

struct Nav2NavigationService::Impl
{
    std::shared_ptr<TurtlebotNavNode> node;
    rclcpp::executors::SingleThreadedExecutor executor;
    std::thread spinThread;
};

Nav2NavigationService::Nav2NavigationService(QObject *parent)
    : NavigationService(parent)
{
    // rclcpp::init() must run before anything touches the global default
    // context — including Impl's SingleThreadedExecutor, whose constructor
    // creates guard conditions against that context. Impl is deliberately
    // NOT built in the member-initializer list (which would run before this
    // body) so ordering stays correct.
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        if (!rclcpp::ok()) {
            int argc = 0;
            rclcpp::init(argc, nullptr);
        }
        // scanPointsUpdated is emitted from the ROS spin thread and queued
        // across to the GUI thread — the container type must be registered
        // for Qt's queued-connection argument marshaling to accept it.
        qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");
    });

    m_impl = std::make_unique<Impl>();
    m_impl->node = std::make_shared<TurtlebotNavNode>(this);
    m_impl->executor.add_node(m_impl->node);
    m_impl->spinThread = std::thread([this]() { m_impl->executor.spin(); });
}

Nav2NavigationService::~Nav2NavigationService()
{
    m_impl->executor.cancel();
    if (m_impl->spinThread.joinable()) {
        m_impl->spinThread.join();
    }
    m_impl->executor.remove_node(m_impl->node);
    m_impl->node.reset();
    if (rclcpp::ok()) {
        rclcpp::shutdown();
    }
}

void Nav2NavigationService::navigateToPose(double x, double y, double yawRad, const QString &frameId)
{
    m_impl->node->navigateToPose(x, y, yawRad, frameId);
}

void Nav2NavigationService::followWaypoints(const QVector<NavPose> &poses, const QString &frameId)
{
    m_impl->node->followWaypoints(poses, frameId);
}

void Nav2NavigationService::cancelGoal()
{
    m_impl->node->cancelGoal();
}

void Nav2NavigationService::publishInitialPose(double x, double y, double yawRad, const QString &frameId)
{
    m_impl->node->publishInitialPose(x, y, yawRad, frameId);
}

void Nav2NavigationService::setSpeedLimit(int percent)
{
    m_impl->node->setSpeedLimit(percent);
}
