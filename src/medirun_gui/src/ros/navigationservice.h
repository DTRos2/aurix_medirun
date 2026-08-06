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
    virtual void navigateToPose(double x, double y, double yawRad, const QString &frameId) = 0;
    virtual void followWaypoints(const QVector<NavPose> &poses, const QString &frameId) = 0;
    virtual void cancelGoal() = 0;
    virtual void retryConnections() = 0;
    virtual void publishInitialPose(double x, double y, double yawRad, const QString &frameId) = 0;
    // percent: 1-100, as a percentage of the controller's configured max
    // speed (Nav2's dynamic speed-limit mechanism — nav2_msgs/SpeedLimit on
    // the "speed_limit" topic, no launch/param changes needed).
    virtual void setSpeedLimit(int percent) = 0;

signals:
    // navigate_to_pose/follow_waypoints 액션 서버가 실제로 goal을 받을 수 있는
    // 상태인지(둘 다 준비됨) — nav2 lifecycle이 inactive(SLAM 매핑 중 PAUSE된
    // 경우 등)면 false. 페이지는 이 신호로 "Nav2 연결" 배지와 주행 버튼
    // 활성화 여부를 결정해야 한다 (statusChanged는 최근 주행 결과/에러 텍스트라
    // 연결 여부를 신뢰성 있게 반영하지 않는다).
    void nav2ConnectionChanged(bool connected);
    void statusChanged(const QString &text);
    void feedbackDistanceRemaining(double metersRemaining);
    void waypointProgress(int currentIndex, int total);
    void logEvent(const QString &level, const QString &message);
    void amclPoseUpdated(double x, double y, double yawRad);
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
