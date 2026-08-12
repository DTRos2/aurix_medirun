#ifndef NAVIGATIONSERVICE_H
#define NAVIGATIONSERVICE_H

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

// (x, y) in meters, yaw in radians — map frame.
struct NavPose
{
    double x {0.0};
    double y {0.0};
    double yaw {0.0};
};

// GUI-side abstraction over Nav2 navigation so pages (waypointmanagementpage,
// remotecontrolpage, ...) never need to include rclcpp/nav2_msgs headers.
// Mirrors the reference pyqt dashboard's RosBridge + TurtlebotNode split:
// this class *is* the Qt signal bridge; the concrete implementation owns
// the rclcpp node/thread and simply calls the (public, since Qt `signals`
// expands to `public`) signal methods from the ROS spin thread.
class NavigationService : public QObject
{
    Q_OBJECT

public:
    explicit NavigationService(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~NavigationService() override = default;

public slots:
    // label: 사람이 읽을 수 있는 목적지/경로 식별자(예: 웨이포인트 이름,
    // 저장된 경로 이름) — DB 기록(robot_session.session_name,
    // user_action_log.payload)에만 쓰인다. 몰라도(빈 문자열) 동작에는 지장 없음.
    virtual void navigateToPose(double x, double y, double yawRad, const QString &frameId, const QString &label = QString()) = 0;
    virtual void followWaypoints(const QVector<NavPose> &poses, const QString &frameId, const QString &label = QString()) = 0;
    virtual void cancelGoal() = 0;
    virtual void retryConnections() = 0;
    virtual void publishInitialPose(double x, double y, double yawRad, const QString &frameId) = 0;
    // percent: 1-100, as a percentage of the controller's configured max
    // speed (Nav2's dynamic speed-limit mechanism — nav2_msgs/SpeedLimit on
    // the "speed_limit" topic, no launch/param changes needed).
    virtual void setSpeedLimit(int percent) = 0;
    // 원격제어 페이지의 teleop(수동 주행)용 — /cmd_vel에 직접 Twist를 발행한다.
    // Nav2 액션(navigateToPose 등)과 별개 경로이므로 goal 진행 중에도 호출 가능.
    virtual void driveManual(double linearX, double angularZ) = 0;

signals:
    // navigate_to_pose/follow_waypoints 액션 서버가 실제로 goal을 받을 수 있는
    // 상태인지(둘 다 준비됨) — nav2 lifecycle이 inactive(SLAM 매핑 중 PAUSE된
    // 경우 등)면 false. 페이지는 이 신호로 "Nav2 연결" 배지와 주행 버튼
    // 활성화 여부를 결정해야 한다 (statusChanged는 최근 주행 결과/에러 텍스트라
    // 연결 여부를 신뢰성 있게 반영하지 않는다).
    void nav2ConnectionChanged(bool connected);
    // 웨이포인트 주행(navigateToPose/followWaypoints)이 실제로 액션 서버에
    // 접수됐을 때 1회 발행 — DB 기록(robot_session/nav_goal_log)의 시작점.
    // goals.size()==1이면 단일 목적지, N개면 경로 주행. label은 호출자가 넘긴
    // 그대로(비어있을 수 있음). 진행 상황은 기존 waypointProgress로, 종료는
    // navSessionFinished로 알린다.
    void navSessionStarted(const QVector<NavPose> &goals, const QString &label);
    // success=true면 도착, false면 실패 — cancelled는 사용자가 명시적으로
    // 취소한 경우만 true(실패와 구분해서 기록하기 위함).
    void navSessionFinished(bool success, bool cancelled);
    void statusChanged(const QString &text);
    void feedbackDistanceRemaining(double metersRemaining);
    void waypointProgress(int currentIndex, int total);
    void logEvent(const QString &level, const QString &message);
    void amclPoseUpdated(double x, double y, double yawRad);
    // /odom 그대로: linearMps는 base_link 전방 기준 선속도(m/s), angularRadps는
    // z축 각속도(rad/s). 로봇 상태 패널의 "속도" 표시와 driving_record 적재용.
    void odomVelocityUpdated(double linearMps, double angularRadps);
    void navigationGoalAccepted();
    void navigationGoalRejected(const QString &reason);
    void navigationResult(const QString &resultCode, const QString &detail);
    void connectionRetryFinished(bool success, const QString &detail);
    // LaserScan (/scan) transformed into the "map" frame — lets the
    // dashboard show obstacle points against the loaded map, same as
    // RViz's LaserScan display, so the user can visually verify the
    // initial-pose alignment. Empty while "map" TF isn't available yet.
    void scanPointsUpdated(const QVector<QPointF> &mapPoints);
};

#endif // NAVIGATIONSERVICE_H
