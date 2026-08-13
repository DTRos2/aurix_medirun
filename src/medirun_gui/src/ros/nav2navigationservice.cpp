#include "nav2navigationservice.h"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <lifecycle_msgs/msg/state.hpp>
#include <lifecycle_msgs/srv/get_state.hpp>
#include <nav2_msgs/action/follow_waypoints.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav2_msgs/msg/speed_limit.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <tf2/exceptions.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <atomic>
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
        recreateInterfaces();
        m_connectionCheckTimer = create_wall_timer(
            std::chrono::milliseconds(1000), [this]() { checkNav2Connection(); });
        checkNav2Connection();
    }

    void recreateInterfaces()
    {
        m_navClient = rclcpp_action::create_client<NavigateToPose>(this, "navigate_to_pose");
        m_followClient = rclcpp_action::create_client<FollowWaypoints>(this, "follow_waypoints");
        m_initialPosePub = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>("initialpose", 10);
        m_speedLimitPub = create_publisher<nav2_msgs::msg::SpeedLimit>("speed_limit", 10);
        m_cmdVelPub = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", rclcpp::QoS(10));

        // amcl은 /amcl_pose를 TRANSIENT_LOCAL(KEEP_LAST 1)로 발행 — 마지막
        // 포즈를 래치해뒀다가 나중에 붙는 구독자에게도 즉시 준다. 여기서
        // VOLATILE(rclcpp::QoS 기본값)로 구독하면 QoS가 안 맞아 그 래치된
        // 과거 메시지를 못 받고, 로봇이 실제로 움직여 amcl이 "새로" 발행할
        // 때까지 로봇 마커가 하나도 안 뜨는 문제가 있었다(Nav2가 이미 초기
        // 위치까지 다 설정된 뒤에 GUI를 나중에 켠 경우 특히). 발행자와 같은
        // QoS로 맞춰서 늦게 켜도 즉시 마지막 위치를 받도록 한다.
        m_amclSub = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
            "amcl_pose", rclcpp::QoS(1).transient_local(),
            [this](const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
                const double yaw = yawFromQuaternion(msg->pose.pose.orientation);
                {
                    std::lock_guard<std::mutex> lock(m_lastAmclPoseMutex);
                    m_lastAmclPoseX = msg->pose.pose.position.x;
                    m_lastAmclPoseY = msg->pose.pose.position.y;
                    m_lastAmclPoseYaw = yaw;
                    m_hasAmclPose = true;
                }
                m_bridge->amclPoseUpdated(msg->pose.pose.position.x, msg->pose.pose.position.y, yaw);
            });

        m_tfBuffer = std::make_unique<tf2_ros::Buffer>(get_clock());
        m_tfListener = std::make_shared<tf2_ros::TransformListener>(*m_tfBuffer);

        m_scanSub = create_subscription<sensor_msgs::msg::LaserScan>(
            "scan", rclcpp::SensorDataQoS(),
            [this](const sensor_msgs::msg::LaserScan::SharedPtr msg) { handleScan(msg); });

        m_odomSub = create_subscription<nav_msgs::msg::Odometry>(
            "odom", rclcpp::QoS(10),
            [this](const nav_msgs::msg::Odometry::SharedPtr msg) {
                m_bridge->odomVelocityUpdated(msg->twist.twist.linear.x, msg->twist.twist.angular.z);
            });

        m_btNavigatorStateClient = create_client<lifecycle_msgs::srv::GetState>("/bt_navigator/get_state");
        m_waypointFollowerStateClient = create_client<lifecycle_msgs::srv::GetState>("/waypoint_follower/get_state");
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

    void navigateToPose(double x, double y, double yaw, const QString &frameId, const QString &label)
    {
        if (m_navGoalHandle || m_followGoalHandle) {
            m_bridge->navigationGoalRejected(QStringLiteral("이미 진행 중인 goal이 있습니다."));
            return;
        }
        if (!m_navClient->wait_for_action_server(std::chrono::seconds(1))) {
            m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("navigate_to_pose 액션 서버를 찾을 수 없습니다."));
            m_bridge->navigationGoalRejected(QStringLiteral("navigate_to_pose 액션 서버 미발견"));
            return;
        }

        NavigateToPose::Goal goal;
        goal.pose = makePoseStamped(x, y, yaw, frameId.toStdString(), now());

        m_bridge->logEvent(QStringLiteral("INFO"),
                            QStringLiteral("단일 목적지 주행 요청: (%1, %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));
        m_bridge->statusChanged(QStringLiteral("목적지 (%1, %2)로 이동 중...").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));
        m_navigationInProgress = true;
        m_bridge->navSessionStarted({NavPose{x, y, yaw}}, label);

        NavClient::SendGoalOptions options;
        options.feedback_callback =
            [this](NavClient::GoalHandle::SharedPtr, const std::shared_ptr<const NavigateToPose::Feedback> feedback) {
                m_bridge->feedbackDistanceRemaining(static_cast<double>(feedback->distance_remaining));
            };
        options.goal_response_callback = [this](NavClient::GoalHandle::SharedPtr handle) {
            if (!handle) {
                m_cancelRequestedBeforeHandle = false;
                m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("목적지 요청이 거부되었습니다."));
                m_bridge->statusChanged(QStringLiteral("대기 중"));
                m_navigationInProgress = false;
                m_bridge->navSessionFinished(false, false);
                m_bridge->navigationGoalRejected(QStringLiteral("navigate_to_pose goal rejected"));
                return;
            }
            m_navGoalHandle = handle;
            if (m_cancelRequestedBeforeHandle) {
                m_cancelRequestedBeforeHandle = false;
                m_navClient->async_cancel_goal(handle);
                return;
            }
            m_bridge->navigationGoalAccepted();
        };
        options.result_callback = [this](const NavClient::GoalHandle::WrappedResult &result) {
            m_navGoalHandle.reset();
            const QString text = resultCodeText(result.code);
            const QString level = (result.code == rclcpp_action::ResultCode::SUCCEEDED)
                                       ? QStringLiteral("INFO")
                                       : QStringLiteral("WARN");
            m_bridge->logEvent(level, QStringLiteral("단일 목적지 주행 결과: %1").arg(text));
            m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: %1)").arg(text));
            m_navigationInProgress = false;
            m_bridge->navSessionFinished(
                result.code == rclcpp_action::ResultCode::SUCCEEDED, result.code == rclcpp_action::ResultCode::CANCELED);
            QString resultCode = QStringLiteral("UNKNOWN");
            if (result.code == rclcpp_action::ResultCode::SUCCEEDED) {
                resultCode = QStringLiteral("SUCCEEDED");
            } else if (result.code == rclcpp_action::ResultCode::CANCELED) {
                resultCode = QStringLiteral("CANCELED");
            } else if (result.code == rclcpp_action::ResultCode::ABORTED) {
                resultCode = QStringLiteral("FAILED");
            }
            m_bridge->navigationResult(resultCode, text);
        };

        m_navClient->async_send_goal(goal, options);
    }

    void followWaypoints(const QVector<NavPose> &poses, const QString &frameId, const QString &label)
    {
        if (m_navGoalHandle || m_followGoalHandle) {
            m_bridge->navigationGoalRejected(QStringLiteral("이미 진행 중인 goal이 있습니다."));
            return;
        }
        if (!m_followClient->wait_for_action_server(std::chrono::seconds(1))) {
            m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("follow_waypoints 액션 서버를 찾을 수 없습니다."));
            m_bridge->navigationGoalRejected(QStringLiteral("follow_waypoints 액션 서버 미발견"));
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
        m_navigationInProgress = true;
        m_bridge->navSessionStarted(poses, label);

        FollowClient::SendGoalOptions options;
        options.feedback_callback =
            [this](FollowClient::GoalHandle::SharedPtr, const std::shared_ptr<const FollowWaypoints::Feedback> feedback) {
                m_bridge->waypointProgress(static_cast<int>(feedback->current_waypoint), m_followTotal);
            };
        options.goal_response_callback = [this](FollowClient::GoalHandle::SharedPtr handle) {
            if (!handle) {
                m_cancelRequestedBeforeHandle = false;
                m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("경로 주행 요청이 거부되었습니다."));
                m_bridge->statusChanged(QStringLiteral("대기 중"));
                m_navigationInProgress = false;
                m_bridge->navSessionFinished(false, false);
                m_bridge->navigationGoalRejected(QStringLiteral("follow_waypoints goal rejected"));
                return;
            }
            m_followGoalHandle = handle;
            if (m_cancelRequestedBeforeHandle) {
                m_cancelRequestedBeforeHandle = false;
                m_followClient->async_cancel_goal(handle);
                return;
            }
            m_bridge->navigationGoalAccepted();
        };
        options.result_callback = [this](const FollowClient::GoalHandle::WrappedResult &result) {
            m_followGoalHandle.reset();
            const bool missedAny = result.result && !result.result->missed_waypoints.empty();
            const bool succeeded = result.code == rclcpp_action::ResultCode::SUCCEEDED && !missedAny;
            if (succeeded) {
                m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("경로 주행 완료"));
                m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: 성공)"));
                m_bridge->navigationResult(QStringLiteral("SUCCEEDED"), QStringLiteral("성공"));
            } else {
                const QString text = resultCodeText(result.code);
                const QString detail = missedAny
                                            ? QStringLiteral(" (놓친 waypoint %1개)").arg(result.result->missed_waypoints.size())
                                            : QString();
                m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("경로 주행 결과: %1%2").arg(text, detail));
                m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: %1)").arg(text));
                const QString resultCode = (result.code == rclcpp_action::ResultCode::CANCELED)
                                               ? QStringLiteral("CANCELED")
                                               : QStringLiteral("FAILED");
                m_bridge->navigationResult(resultCode, text + detail);
            }
            m_navigationInProgress = false;
            m_bridge->navSessionFinished(succeeded, result.code == rclcpp_action::ResultCode::CANCELED);
        };

        m_followClient->async_send_goal(goal, options);
    }

    void cancelGoal()
    {
        if (m_navGoalHandle) {
            m_navClient->async_cancel_goal(m_navGoalHandle);
            m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("주행 취소를 요청했습니다."));
            m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
            return;
        }
        if (m_followGoalHandle) {
            m_followClient->async_cancel_goal(m_followGoalHandle);
            m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("주행 취소를 요청했습니다."));
            m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
            return;
        }
        if (m_navigationInProgress) {
            // navigateToPose/followWaypoints를 호출한 직후 goal_response_callback이
            // 아직 안 와서(accept 대기 중) 핸들이 잠깐 비어 있는 순간이 있다. 여기서
            // 그냥 무시하면 취소 요청이 통째로 씹혀서 "전환"을 눌러도 로봇이 원래
            // 목적지로 계속 가버린다.
            m_cancelRequestedBeforeHandle = true;
            m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("주행 취소를 요청했습니다. (다음 목표 수신 즉시 취소)"));
            m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
            return;
        }
        m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("취소할 진행 중인 주행이 없습니다."));
    }

    void retryConnections()
    {
        recreateInterfaces();
        checkNav2Connection();
        const auto topics = get_topic_names_and_types();
        const bool hasAmcl = topics.find("amcl_pose") != topics.end();
        const bool hasScan = topics.find("scan") != topics.end();
        const QString detail = QStringLiteral("nav2=%1 amcl_pose=%2 scan=%3")
                                   .arg(m_nav2Connected ? QStringLiteral("ok") : QStringLiteral("pending"))
                                   .arg(hasAmcl ? QStringLiteral("ok") : QStringLiteral("missing"))
                                   .arg(hasScan ? QStringLiteral("ok") : QStringLiteral("missing"));
        m_bridge->connectionRetryFinished(m_nav2Connected, detail);
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

    // teleop 재발행 타이머(100ms)에서 반복 호출되므로 매번 logEvent를 보내지
    // 않는다 — publishInitialPose/setSpeedLimit과 달리 1회성 호출이 아니다.
    void driveManual(double linearX, double angularZ)
    {
        geometry_msgs::msg::Twist msg;
        msg.linear.x = linearX;
        msg.angular.z = angularZ;
        m_cmdVelPub->publish(msg);
    }

    // GUI 스레드에서 직접 호출 — m_nav2Connected가 atomic이라 락 없이 안전하다.
    bool isConnected() const
    {
        return m_nav2Connected;
    }

    // GUI 스레드에서 직접 호출 — m_navigationInProgress가 atomic이라 락 없이
    // 안전하다. navSessionStarted/navSessionFinished 시그널을 놓친 뒤늦은
    // 구독자(주행 도중에 페이지가 열린 경우)를 위한 현재 상태 조회.
    bool isNavigating() const
    {
        return m_navigationInProgress;
    }

    bool lastKnownAmclPose(double &x, double &y, double &yawRad) const
    {
        std::lock_guard<std::mutex> lock(m_lastAmclPoseMutex);
        if (!m_hasAmclPose) {
            return false;
        }
        x = m_lastAmclPoseX;
        y = m_lastAmclPoseY;
        yawRad = m_lastAmclPoseYaw;
        return true;
    }

    // bt_navigator/waypoint_follower 둘 다 GetState 서비스가 응답 가능하고
    // active여야 "연결됨"으로 본다. 두 서비스 응답이 비동기로 따로 도착하므로
    // 라운드마다(m_connectionCheckRound) 결과를 모아서 마지막에 한 번만 판정한다
    // — 서로 다른 라운드의 응답이 섞이면 오판정할 수 있어 라운드 번호로 구분한다.
    void checkNav2Connection()
    {
        if (!m_btNavigatorStateClient->service_is_ready() || !m_waypointFollowerStateClient->service_is_ready()) {
            registerConnectionResult(false);
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
        registerConnectionResult(m_btNavigatorActiveForRound.second && m_waypointFollowerActiveForRound.second);
    }

    // 초기 위치 설정(퍼블리시) 직후처럼 AMCL/bt_navigator에 순간적으로 부하가
    // 걸리면 GetState 서비스 응답이 1초 폴링 주기를 넘겨 한 라운드 정도 실패할
    // 수 있다. 그 한 번으로 바로 배지를 꺼버리면 "붙었다 꺼졌다" 깜빡임으로
    // 보이므로, RosSensorTelemetryProvider/RosBatteryTelemetryProvider가 쓰는
    // 것과 같은 디바운스 방식을 적용한다 — 연속 실패가 임계치를 넘어야만
    // "끊김"으로 확정하고, 성공은 즉시 반영해 복귀는 빠르게 보이게 한다.
    void registerConnectionResult(bool connectedNow)
    {
        if (connectedNow) {
            m_disconnectStreak = 0;
            updateNav2Connected(true);
            return;
        }
        if (++m_disconnectStreak >= kDisconnectDebounceRounds) {
            updateNav2Connected(false);
        }
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
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr m_cmdVelPub;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr m_amclSub;
    // GUI 스레드의 lastKnownAmclPose() 조회와 ROS 스핀 스레드의 구독 콜백이
    // 동시에 접근하므로 mutex로 보호한다.
    mutable std::mutex m_lastAmclPoseMutex;
    double m_lastAmclPoseX {0.0};
    double m_lastAmclPoseY {0.0};
    double m_lastAmclPoseYaw {0.0};
    bool m_hasAmclPose {false};
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr m_scanSub;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr m_odomSub;
    std::unique_ptr<tf2_ros::Buffer> m_tfBuffer;
    std::shared_ptr<tf2_ros::TransformListener> m_tfListener;
    NavClient::GoalHandle::SharedPtr m_navGoalHandle;
    FollowClient::GoalHandle::SharedPtr m_followGoalHandle;
    // goal 핸들이 아직 없는 순간에 cancelGoal()이 호출되면(예: 대기 호출 "전환"
    // 버튼) 취소할 대상이 없어 요청이 그냥 씹혔다. 이 플래그를 세워두면 다음
    // goal이 accept되는 즉시 취소를 적용한다.
    bool m_cancelRequestedBeforeHandle {false};
    int m_followTotal {0};
    rclcpp::TimerBase::SharedPtr m_connectionCheckTimer;
    rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedPtr m_btNavigatorStateClient;
    rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedPtr m_waypointFollowerStateClient;
    uint64_t m_connectionCheckRound {0};
    // {round, active} — 그 라운드에서 마지막으로 받은 응답. round가 현재
    // m_connectionCheckRound와 다르면 아직 이번 라운드 응답이 안 온 것.
    std::pair<uint64_t, bool> m_btNavigatorActiveForRound {0, false};
    std::pair<uint64_t, bool> m_waypointFollowerActiveForRound {0, false};
    // ROS 스핀 스레드에서 쓰고, isNav2Connected()로 GUI 스레드에서 읽으므로
    // atomic — 시그널을 놓친 뒤늦은 구독자가 현재 상태를 안전하게 조회할 수
    // 있어야 한다(setNavigationService()가 연결이 이미 성사된 뒤에 불리는
    // 경우의 레이스 컨디션 대응).
    std::atomic<bool> m_nav2Connected {false};
    // navSessionStarted()/navSessionFinished() 발행 시점에 맞춰 같이
    // 갱신한다 — isNavigating()으로 GUI 스레드에서 안전하게 조회.
    std::atomic<bool> m_navigationInProgress {false};
    // checkNav2Connection()이 1초마다 도니 3회 연속 실패 ≈ 3초 미수신을 뜻한다.
    // 03번 설계서 1.5절 통신 규칙과 같은 "일정 시간 미수신 시 단절" 취지.
    static constexpr int kDisconnectDebounceRounds = 3;
    int m_disconnectStreak {0};
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
}

void Nav2NavigationService::navigateToPose(double x, double y, double yawRad, const QString &frameId, const QString &label)
{
    m_impl->node->navigateToPose(x, y, yawRad, frameId, label);
}

void Nav2NavigationService::followWaypoints(const QVector<NavPose> &poses, const QString &frameId, const QString &label)
{
    m_impl->node->followWaypoints(poses, frameId, label);
}

void Nav2NavigationService::cancelGoal()
{
    m_impl->node->cancelGoal();
}

void Nav2NavigationService::retryConnections()
{
    m_impl->node->retryConnections();
}

void Nav2NavigationService::publishInitialPose(double x, double y, double yawRad, const QString &frameId)
{
    m_impl->node->publishInitialPose(x, y, yawRad, frameId);
}

void Nav2NavigationService::driveManual(double linearX, double angularZ)
{
    m_impl->node->driveManual(linearX, angularZ);
}

void Nav2NavigationService::setSpeedLimit(int percent)
{
    m_impl->node->setSpeedLimit(percent);
}

bool Nav2NavigationService::isNav2Connected() const
{
    return m_impl->node->isConnected();
}

bool Nav2NavigationService::lastKnownAmclPose(double &x, double &y, double &yawRad) const
{
    return m_impl->node->lastKnownAmclPose(x, y, yawRad);
}

bool Nav2NavigationService::isNavigating() const
{
    return m_impl->node->isNavigating();
}
