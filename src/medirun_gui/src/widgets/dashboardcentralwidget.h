#ifndef DASHBOARDCENTRALWIDGET_H
#define DASHBOARDCENTRALWIDGET_H

#include "../app/appsession.h"
#include "../models/telemetrymodel.h"

#include <QWidget>

class AuthService;
class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;
class QLineEdit;

class DashboardCentralWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardCentralWidget(const AppSession &session, AuthService &authService, QWidget *parent = nullptr);

    void applySnapshot(const TelemetrySnapshot &snapshot);

signals:
    void exitRequested();
    void estopRequested();
    void missionCancelRequested();
    void returnHomeRequested();
    void manualControlRequested();
    void logoutRequested();

private:
    QWidget *buildAdminView();
    QWidget *buildAdminSidebar();
    QWidget *buildAdminHeader();
    QWidget *buildAdminOverviewPage();
    QWidget *buildAdminRobotsPage();
    QWidget *buildAdminMissionsPage();
    QWidget *buildAdminHistoryPage();
    QWidget *buildAdminUsersPage();
    QWidget *buildAdminSettingsPage();
    QWidget *buildAdminMetricCard(const QString &title, QLabel *&valueLabel, const QString &accentName, const QString &caption);
    QWidget *buildAdminRobotOverview();
    QWidget *buildAdminHealthPanel();
    QWidget *buildAdminEventsPanel();
    QWidget *buildUserView();
    QWidget *buildUserPageHeader();
    QWidget *buildUserNavBar();
    QWidget *buildUserMetricCard(const QString &title, QLabel *&valueLabel, const QString &accentName, const QString &caption);
    QWidget *buildUserDashboardPage();
    QWidget *buildUserRobotPanel();
    QWidget *buildUserMissionPanel();
    QWidget *buildUserRobotPage();
    QWidget *buildUserSensorPage();
    QWidget *buildUserCameraPage();
    QWidget *buildUserMapPage();
    QWidget *buildUserSshAgentPage();
    QWidget *buildUserLogReplayPage();
    QWidget *buildAdminInfoPage(const QString &title, const QString &subtitle, const QStringList &items);
    QWidget *buildUserManagementForm();
    void refreshUserTable();
    void handleCreateUser();
    void switchAdminSection(int index);
    void switchUserSection(int index);
    void setBadgeStyle(QWidget *widget, const QString &kind);
    void updateAdminSnapshot(const TelemetrySnapshot &snapshot);
    void updateUserSnapshot(const TelemetrySnapshot &snapshot);
    QString statePill(const QString &text, const QString &kind) const;
    QString formatRecentMissionLines(const QVector<SystemEventItem> &events) const;

    AuthService &m_authService;
    AppSession m_session;
    QVBoxLayout *m_rootLayout;
    QLabel *m_adminTitleLabel;
    QLabel *m_adminDateLabel;
    QLabel *m_adminSystemBadge;
    QLabel *m_adminTotalRobotsValue;
    QLabel *m_adminActiveMissionValue;
    QLabel *m_adminTodayCallsValue;
    QLabel *m_adminSafetyEventsValue;
    QStackedWidget *m_adminStack;
    QStackedWidget *m_userStack;
    QTableWidget *m_adminRobotTable;
    QLabel *m_adminHealthRosValue;
    QLabel *m_adminHealthNavValue;
    QLabel *m_adminHealthDbValue;
    QLabel *m_adminHealthNetValue;
    QTableWidget *m_adminEventsTable;
    QLineEdit *m_userCreateEmailEdit;
    QLineEdit *m_userCreatePasswordEdit;
    QComboBox *m_userCreateRoleCombo;
    QLabel *m_userCreateStatusLabel;
    QTableWidget *m_userManagementTable;
    QList<QPushButton *> m_adminNavButtons;
    QList<QPushButton *> m_userNavButtons;
    QLabel *m_userOnlineBadge;
    QLabel *m_userDescriptionLabel;
    QLabel *m_userAssignedRobotValue;
    QLabel *m_userBatteryValue;
    QLabel *m_userStateValue;
    QLabel *m_userMissionCountValue;
    QLabel *m_userConnectionValue;
    QLabel *m_userRobotIdLabel;
    QLabel *m_userRobotSubtitleLabel;
    QLabel *m_userRobotStatusLabel;
    QLabel *m_userHeartbeatLabel;
    QLabel *m_userHealthTagsLabel;
    QLabel *m_userMissionLogLabel;
    QLabel *m_userRobotPageTitleLabel;
    QLabel *m_userRobotDetailLabel;
    QTableWidget *m_userRobotDeviceTable;
    QTableWidget *m_userSensorTable;
    QLabel *m_userCameraStatusLabel;
    QLabel *m_userCameraTopicLabel;
    QLabel *m_userCameraEventsLabel;
    QLabel *m_userMapMissionLabel;
    QLabel *m_userMapPositionLabel;
    QLabel *m_userMapDistanceLabel;
    QLabel *m_userSshStatusLabel;
    QLabel *m_userSshNetworkLabel;
    QLabel *m_userSshDbLabel;
    QTableWidget *m_userLogTable;
    QPushButton *m_manualControlButton;
    QPushButton *m_waypointOneButton;
    QPushButton *m_waypointTwoButton;
    QPushButton *m_waypointThreeButton;
};

#endif // DASHBOARDCENTRALWIDGET_H
