#include "nav2navigationservice.h"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <lifecycle_msgs/msg/state.hpp>
#include <lifecycle_msgs/srv/get_state.hpp>
#include <nav2_msgs/action/follow_waypoints.hpp>
#include <nav2_msgs/action/navigate_through_poses.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <nav2_msgs/msg/speed_limit.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <tf2/exceptions.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace {

// 트레일러 정렬용 접근(approach) 지점 거리(L)의 기본값과 축소 폭.
// 이 두 값만 바꾸면 전체 접근거리 후보 목록이 같이 바뀐다. 기본값이
// 장애물/미탐색 영역에 걸리면 한 단계씩 줄여가며 시도한다.
constexpr double kDefaultApproachDistanceMeters = 0.50;
constexpr double kApproachDistanceStepMeters = 0.10;
constexpr int kApproachDistanceFallbackSteps = 2; // 0.50 -> 0.40 -> 0.30
// OccupancyGrid 셀 비용(0~100, -1=미탐색) 중 이 값 이상이면 장애물에 가까운
// 영역으로 보고 approach pose 후보에서 제외한다.
constexpr int8_t kApproachLethalCostThreshold = 90;

std::vector<double> approachDistanceCandidatesMeters()
{
    std::vector<double> candidates;
    for (int i = 0; i <= kApproachDistanceFallbackSteps; ++i) {
        candidates.push_back(kDefaultApproachDistanceMeters - i * kApproachDistanceStepMeters);
    }
    return candidates;
}

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

geometry_msgs::msg::PoseStamped makeApproachPoseStamped(double goalX, double goalY, double goalYaw,
                                                        double distance, const std::string &frameId,
                                                        const rclcpp::Time &stamp)
{
    const double approachX = goalX - distance * std::cos(goalYaw);
    const double approachY = goalY - distance * std::sin(goalYaw);
    return makePoseStamped(approachX, approachY, goalYaw, frameId, stamp);
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
    using NavigateThroughPoses = nav2_msgs::action::NavigateThroughPoses;
    using NavClient = rclcpp_action::Client<NavigateToPose>;
    using FollowClient = rclcpp_action::Client<FollowWaypoints>;
    using ThroughPosesClient = rclcpp_action::Client<NavigateThroughPoses>;

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
        m_throughPosesClient = rclcpp_action::create_client<NavigateThroughPoses>(this, "navigate_through_poses");
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

        m_costmapSub = create_subscription<nav_msgs::msg::OccupancyGrid>(
            "global_costmap/costmap", rclcpp::QoS(1).transient_local(),
            [this](const nav_msgs::msg::OccupancyGrid::SharedPtr msg) { m_latestCostmap = msg; });

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

    bool isApproachPoseTraversable(double x, double y) const
    {
        if (!m_latestCostmap) {
            return true;
        }
        const auto &info = m_latestCostmap->info;
        const int mx = static_cast<int>(std::floor((x - info.origin.position.x) / info.resolution));
        const int my = static_cast<int>(std::floor((y - info.origin.position.y) / info.resolution));
        if (mx < 0 || my < 0 || mx >= static_cast<int>(info.width) || my >= static_cast<int>(info.height)) {
            return false;
        }
        const int8_t cost = m_latestCostmap->data[static_cast<size_t>(my) * info.width + static_cast<size_t>(mx)];
        return cost >= 0 && cost < kApproachLethalCostThreshold;
    }

    std::optional<geometry_msgs::msg::PoseStamped> findApproachPose(
        double goalX, double goalY, double goalYaw, const std::string &frameId, const rclcpp::Time &stamp,
        const QString &labelForLog)
    {
        for (const double candidate : approachDistanceCandidatesMeters()) {
            const double ax = goalX - candidate * std::cos(goalYaw);
            const double ay = goalY - candidate * std::sin(goalYaw);
            if (isApproachPoseTraversable(ax, ay)) {
                if (candidate < kDefaultApproachDistanceMeters) {
                    m_bridge->logEvent(
                        QStringLiteral("WARN"),
                        QStringLiteral("%1: 기본 접근거리 %2m가 장애물/미탐색 영역에 걸려 %3m로 줄였습니다.")
                            .arg(labelForLog)
                            .arg(kDefaultApproachDistanceMeters, 0, 'f', 2)
                            .arg(candidate, 0, 'f', 2));
                }
                return makeApproachPoseStamped(goalX, goalY, goalYaw, candidate, frameId, stamp);
            }
        }
        m_bridge->logEvent(QStringLiteral("WARN"),
                           QStringLiteral("%1: 트레일러 정렬용 접근 지점을 찾지 못해 최종 목표로 바로 이동합니다.")
                               .arg(labelForLog));
        return std::nullopt;
    }

    void navigateToPose(double x, double y, double yaw, const QString &frameId, const QString &label)
    {
        // m_navGoalHandle만 보면 안 된다 — 트레일러 접근(approach) 지점을 먼저
        // 들렀다 최종 목표로 가는 2단계 주행 중, 1단계 goal이 성공해
        // sendNavGoalLeg()의 result_callback이 m_navGoalHandle.reset()을 부른
        // 뒤 2단계 goal의 goal_response_callback이 도착하기 전까지는
        // m_navGoalHandle이 잠깐 비어 있다. 그 틈에 새 navigateToPose 호출이
        // 들어오면 이 가드를 통과해 같은 로봇에 두 번째 goal을 보내버리고,
        // Nav2가 새 goal을 받아들이며 기존 1건을 preempt해서 "먼저 누른 호출이
        // 사라지고 나중 호출이 바로 실행된다"처럼 보인다. m_navigationInProgress는
        // 이 2단계 전체가 끝날 때만 false가 되는 atomic 플래그라 그 틈을 덮는다.
        if (m_navGoalHandle || m_followGoalHandle || m_throughPosesGoalHandle || m_navigationInProgress) {
            m_bridge->navigationGoalRejected(QStringLiteral("이미 진행 중인 goal이 있습니다."));
            return;
        }
        if (!m_navClient->wait_for_action_server(std::chrono::seconds(1))) {
            m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("navigate_to_pose 액션 서버를 찾을 수 없습니다."));
            m_bridge->navigationGoalRejected(QStringLiteral("navigate_to_pose 액션 서버 미발견"));
            return;
        }

        const rclcpp::Time stamp = now();
        const std::string frameStd = frameId.toStdString();
        const QString labelForLog = label.isEmpty() ? QStringLiteral("목적지") : label;
        const auto approachPose = findApproachPose(x, y, yaw, frameStd, stamp, labelForLog);
        const auto finalPose = makePoseStamped(x, y, yaw, frameStd, stamp);

        m_bridge->logEvent(QStringLiteral("INFO"),
                            QStringLiteral("단일 목적지 주행 요청: (%1, %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));
        m_bridge->statusChanged(QStringLiteral("목적지 (%1, %2)로 이동 중...").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));
        m_navigationInProgress = true;
        m_bridge->navSessionStarted({NavPose{x, y, yaw}}, label);

        if (!approachPose.has_value()) {
            sendNavGoalLeg(finalPose, true, nullptr);
            return;
        }

        sendNavGoalLeg(*approachPose, false, [this, finalPose]() {
            sendNavGoalLeg(finalPose, true, nullptr);
        });
    }

    void sendNavGoalLeg(const geometry_msgs::msg::PoseStamped &pose, bool isFinalLeg,
                        std::function<void()> onSuccess)
    {
        NavigateToPose::Goal goal;
        goal.pose = pose;

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
        options.result_callback = [this, isFinalLeg, onSuccess](const NavClient::GoalHandle::WrappedResult &result) {
            m_navGoalHandle.reset();
            if (!isFinalLeg && result.code == rclcpp_action::ResultCode::SUCCEEDED) {
                onSuccess();
                return;
            }
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
        // navigateToPose()와 동일한 이유로 m_navigationInProgress도 같이 본다.
        if (m_navGoalHandle || m_followGoalHandle || m_throughPosesGoalHandle || m_navigationInProgress) {
            m_bridge->navigationGoalRejected(QStringLiteral("이미 진행 중인 goal이 있습니다."));
            return;
        }
        if (!m_followClient->wait_for_action_server(std::chrono::seconds(1))) {
            m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("follow_waypoints 액션 서버를 찾을 수 없습니다."));
            m_bridge->navigationGoalRejected(QStringLiteral("follow_waypoints 액션 서버 미발견"));
            return;
        }

        const rclcpp::Time stamp = now();
        const std::string frameStd = frameId.toStdString();
        FollowWaypoints::Goal goal;
        goal.poses.reserve(static_cast<size_t>(poses.size()) * 2);
        m_followRealPoseIndices.clear();
        for (const NavPose &pose : poses) {
            const QString labelForLog = QStringLiteral("웨이포인트 %1").arg(m_followRealPoseIndices.size() + 1);
            const auto approachPose = findApproachPose(pose.x, pose.y, pose.yaw, frameStd, stamp, labelForLog);
            if (approachPose.has_value()) {
                goal.poses.push_back(*approachPose);
            }
            m_followRealPoseIndices.push_back(goal.poses.size());
            goal.poses.push_back(makePoseStamped(pose.x, pose.y, pose.yaw, frameStd, stamp));
        }
        m_followTotal = static_cast<int>(poses.size());

        m_bridge->logEvent(QStringLiteral("INFO"),
                           QStringLiteral("경로 주행 요청: waypoint %1개 (접근 지점 포함 총 %2개 지점 경유)")
                               .arg(m_followTotal)
                               .arg(goal.poses.size()));
        m_bridge->waypointProgress(0, m_followTotal);
        m_bridge->statusChanged(QStringLiteral("경로 주행 중 (0/%1)").arg(m_followTotal));
        m_navigationInProgress = true;
        m_bridge->navSessionStarted(poses, label);

        FollowClient::SendGoalOptions options;
        options.feedback_callback =
            [this](FollowClient::GoalHandle::SharedPtr, const std::shared_ptr<const FollowWaypoints::Feedback> feedback) {
                const auto currentWaypoint = feedback->current_waypoint;
                const int realCompleted = static_cast<int>(std::count_if(
                    m_followRealPoseIndices.begin(), m_followRealPoseIndices.end(),
                    [currentWaypoint](size_t idx) { return idx < currentWaypoint; }));
                m_bridge->waypointProgress(realCompleted, m_followTotal);
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

    // trajectory(+HOME) 등 여러 점을 중간 정지 없이 하나의 부드러운 경로로 잇는다.
    // followWaypoints()와 마찬가지로 각 실제 지점 앞에 (가능하면) approach pose를
    // 끼워넣는다 — 안 그러면 코너에서 방향을 크게 꺾어야 하는 지점(예: BTN_01
    // 진입로 ㄱ자 코너)에서 트레일러가 그 방향 전환을 못 따라가 제자리 회전만
    // 하다 멈추는 문제가 있었다(2026-08-24). approach pose도 그냥 지나가는 경유점
    // 중 하나로 들어갈 뿐이라 NavigateThroughPoses의 "중간 정지 없음" 특성은
    // 그대로 유지된다.
    void navigateThroughPoses(const QVector<NavPose> &poses, const QString &frameId, const QString &label)
    {
        if (m_navGoalHandle || m_followGoalHandle || m_throughPosesGoalHandle || m_navigationInProgress) {
            m_bridge->navigationGoalRejected(QStringLiteral("이미 진행 중인 goal이 있습니다."));
            return;
        }
        if (poses.isEmpty()) {
            m_bridge->navigationGoalRejected(QStringLiteral("경로에 웨이포인트가 없습니다."));
            return;
        }
        if (!m_throughPosesClient->wait_for_action_server(std::chrono::seconds(1))) {
            m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("navigate_through_poses 액션 서버를 찾을 수 없습니다."));
            m_bridge->navigationGoalRejected(QStringLiteral("navigate_through_poses 액션 서버 미발견"));
            return;
        }

        const rclcpp::Time stamp = now();
        const std::string frameStd = frameId.toStdString();
        NavigateThroughPoses::Goal goal;
        goal.poses.reserve(static_cast<size_t>(poses.size()) * 2);
        m_throughRealPoseIndices.clear();
        for (const NavPose &pose : poses) {
            const QString labelForLog = QStringLiteral("경유지점 %1").arg(m_throughRealPoseIndices.size() + 1);
            const auto approachPose = findApproachPose(pose.x, pose.y, pose.yaw, frameStd, stamp, labelForLog);
            if (approachPose.has_value()) {
                goal.poses.push_back(*approachPose);
            }
            m_throughRealPoseIndices.push_back(goal.poses.size());
            goal.poses.push_back(makePoseStamped(pose.x, pose.y, pose.yaw, frameStd, stamp));
        }
        m_throughPosesTotal = static_cast<int>(poses.size());
        const int sentTotal = static_cast<int>(goal.poses.size());

        m_bridge->logEvent(QStringLiteral("INFO"),
                           QStringLiteral("경로 주행 요청: %1개 지점 연속 주행 (접근 지점 포함 총 %2개 지점 경유)")
                               .arg(m_throughPosesTotal)
                               .arg(sentTotal));
        m_bridge->waypointProgress(0, m_throughPosesTotal);
        m_bridge->statusChanged(QStringLiteral("경로 주행 중 (0/%1)").arg(m_throughPosesTotal));
        m_navigationInProgress = true;
        m_bridge->navSessionStarted(poses, label);

        ThroughPosesClient::SendGoalOptions options;
        options.feedback_callback = [this, sentTotal](ThroughPosesClient::GoalHandle::SharedPtr,
                                           const std::shared_ptr<const NavigateThroughPoses::Feedback> feedback) {
            const int remaining = static_cast<int>(feedback->number_of_poses_remaining);
            const int passedCount = std::max(0, sentTotal - remaining);
            const int realCompleted = static_cast<int>(std::count_if(
                m_throughRealPoseIndices.begin(), m_throughRealPoseIndices.end(),
                [passedCount](size_t idx) { return static_cast<int>(idx) < passedCount; }));
            m_bridge->waypointProgress(realCompleted, m_throughPosesTotal);
        };
        options.goal_response_callback = [this](ThroughPosesClient::GoalHandle::SharedPtr handle) {
            if (!handle) {
                m_cancelRequestedBeforeHandle = false;
                m_bridge->logEvent(QStringLiteral("ERROR"), QStringLiteral("경로 주행 요청이 거부되었습니다."));
                m_bridge->statusChanged(QStringLiteral("대기 중"));
                m_navigationInProgress = false;
                m_bridge->navSessionFinished(false, false);
                m_bridge->navigationGoalRejected(QStringLiteral("navigate_through_poses goal rejected"));
                return;
            }
            m_throughPosesGoalHandle = handle;
            if (m_cancelRequestedBeforeHandle) {
                m_cancelRequestedBeforeHandle = false;
                m_throughPosesClient->async_cancel_goal(handle);
                return;
            }
            m_bridge->navigationGoalAccepted();
        };
        options.result_callback = [this](const ThroughPosesClient::GoalHandle::WrappedResult &result) {
            m_throughPosesGoalHandle.reset();
            const bool succeeded = result.code == rclcpp_action::ResultCode::SUCCEEDED;
            if (succeeded) {
                m_bridge->logEvent(QStringLiteral("INFO"), QStringLiteral("경로 주행 완료"));
                m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: 성공)"));
                m_bridge->navigationResult(QStringLiteral("SUCCEEDED"), QStringLiteral("성공"));
            } else {
                const QString text = resultCodeText(result.code);
                m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("경로 주행 결과: %1").arg(text));
                m_bridge->statusChanged(QStringLiteral("대기 중 (마지막 결과: %1)").arg(text));
                const QString resultCode = (result.code == rclcpp_action::ResultCode::CANCELED)
                                               ? QStringLiteral("CANCELED")
                                               : QStringLiteral("FAILED");
                m_bridge->navigationResult(resultCode, text);
            }
            m_navigationInProgress = false;
            m_bridge->navSessionFinished(succeeded, result.code == rclcpp_action::ResultCode::CANCELED);
        };

        m_throughPosesClient->async_send_goal(goal, options);
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
        if (m_throughPosesGoalHandle) {
            m_throughPosesClient->async_cancel_goal(m_throughPosesGoalHandle);
            m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("주행 취소를 요청했습니다."));
            m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
            return;
        }
        if (m_navigationInProgress) {

            // approach 목표가 막 끝나고 최종 목표가 아직 accept되지 않은 순간처럼
            // 핸들이 잠깐 비어 있을 수 있다. 여기서 그냥 무시하면 취소 요청이
            // 통째로 씹혀서 "전환"을 눌러도 로봇이 원래 목적지로 계속 가버린다.
            m_cancelRequestedBeforeHandle = true;
            m_bridge->logEvent(QStringLiteral("WARN"), QStringLiteral("주행 취소를 요청했습니다. (다음 목표 수신 즉시 취소)"));
            m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
            return;
        }

        // 이 프로세스가 추적 중인 goal 핸들은 없지만, GUI가 미션 도중에
        // 재시작됐다면 이전 세션이 보낸 goal은 액션 서버(Nav2) 쪽에서 클라이언트
        // 생사와 무관하게 계속 실행 중일 수 있다 — 새로 뜬 이 프로세스는 그
        // goal을 전혀 모르니 로컬 핸들 기반 취소로는 못 멈춘다. 로컬 추적 유무와
        // 상관없이 액션 서버에 남아있는 모든 goal을 취소 요청해 이런 orphan
        // 상황도 확실히 멈추게 한다(추적 중인 게 진짜 없으면 Nav2가 그냥 무시).
        m_navClient->async_cancel_all_goals();
        m_followClient->async_cancel_all_goals();
        m_throughPosesClient->async_cancel_all_goals();
        m_bridge->logEvent(QStringLiteral("WARN"),
                           QStringLiteral("이 세션이 추적 중인 주행은 없어, 혹시 남아있을 이전 목표를 전체 취소 요청했습니다."));
        m_bridge->statusChanged(QStringLiteral("취소 요청됨..."));
        // 이 세션은 orphan된 goal의 핸들이 없어 그 goal 자체의 result_callback을
        // 받을 방법이 없다 — 그대로 두면 MissionController가 Cancelling 상태에서
        // 영원히 CANCELED를 못 받아 멈춘다. 위에서 전체 취소를 요청한 것으로
        // 갈음해 이 세션 기준으로는 바로 취소 완료 처리한다.
        m_bridge->navigationResult(QStringLiteral("CANCELED"),
                                   QStringLiteral("이전 세션의 미추적 목표 — 전체 취소 요청으로 갈음"));
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
    ThroughPosesClient::SharedPtr m_throughPosesClient;
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
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr m_costmapSub;
    nav_msgs::msg::OccupancyGrid::SharedPtr m_latestCostmap;
    std::unique_ptr<tf2_ros::Buffer> m_tfBuffer;
    std::shared_ptr<tf2_ros::TransformListener> m_tfListener;
    NavClient::GoalHandle::SharedPtr m_navGoalHandle;
    FollowClient::GoalHandle::SharedPtr m_followGoalHandle;
    ThroughPosesClient::GoalHandle::SharedPtr m_throughPosesGoalHandle;
    int m_throughPosesTotal {0};
    // followWaypoints()의 m_followRealPoseIndices와 동일한 목적 — goal.poses
    // 안에서 "실제 지점"이 위치한 인덱스들(approach pose 제외). feedback의
    // number_of_poses_remaining을 실제 지점 개수 기준 진행률로 환산하는 데 쓴다.
    std::vector<size_t> m_throughRealPoseIndices;

    // approach->최종 목표 전환 구간처럼 goal 핸들이 아직 없는 순간에 cancelGoal()이
    // 호출되면(예: 대기 호출 "전환" 버튼) 취소할 대상이 없어 요청이 그냥 씹혔다.
    // 이 플래그를 세워두면 다음 goal이 accept되는 즉시 취소를 적용한다.
    bool m_cancelRequestedBeforeHandle {false};
    int m_followTotal {0};
    std::vector<size_t> m_followRealPoseIndices;
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

void Nav2NavigationService::navigateThroughPoses(const QVector<NavPose> &poses, const QString &frameId, const QString &label)
{
    m_impl->node->navigateThroughPoses(poses, frameId, label);
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
