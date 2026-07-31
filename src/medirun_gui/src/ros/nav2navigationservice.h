#ifndef NAV2NAVIGATIONSERVICE_H
#define NAV2NAVIGATIONSERVICE_H

#include "navigationservice.h"

#include <memory>

// rclcpp-backed NavigationService. All ROS2 types are confined to the .cpp
// (pimpl) so headers that only need to talk to a NavigationService pointer
// (e.g. waypointmanagementpage.h) never pull in rclcpp/nav2_msgs.
//
// A background thread spins a dedicated rclcpp::Node while this object
// (created and living on the Qt/GUI thread) exposes the NavigationService
// signal API; the node calls those (public, per Qt's `signals` == `public`)
// signal methods directly from the spin thread, matching the reference pyqt
// dashboard's RosBridge/TurtlebotNode split.
class Nav2NavigationService : public NavigationService
{
    Q_OBJECT

public:
    explicit Nav2NavigationService(QObject *parent = nullptr);
    ~Nav2NavigationService() override;

public slots:
    void navigateToPose(double x, double y, double yawRad, const QString &frameId) override;
    void followWaypoints(const QVector<NavPose> &poses, const QString &frameId) override;
    void cancelGoal() override;
    void publishInitialPose(double x, double y, double yawRad, const QString &frameId) override;
    void setSpeedLimit(int percent) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // NAV2NAVIGATIONSERVICE_H
