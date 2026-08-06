#ifndef NAVGOALLOGRECORDER_H
#define NAVGOALLOGRECORDER_H

#include "../ros/navigationservice.h"

#include <QObject>

class RobotService;
class OperationsOverviewService;

// NavigationService::navSessionStarted/waypointProgress/navSessionFinished를
// 구독해 robot_session/nav_goal_log에 기록한다. TelemetryLogRecorder와 같은
// 패턴 — 등록된 로봇이 정확히 1대일 때만 기록한다(여러 대면 이 신호가 어느
// 로봇 것인지 특정할 수 없어 건너뜀).
//
// 알려진 한계: 이전 주행이 끝나기 전에 새 주행을 시작하면(예: 취소 없이 바로
// 다른 목적지로 재요청) navSessionFinished가 어느 세션에 대한 것인지 구분할
// 방법이 없어 최신 세션 하나만 추적한다 — 클래스 프로젝트 규모의 단일 로봇
// 사용 시나리오에서는 발생 가능성이 낮다고 보고 더 정교한 세션 상관관계
// 추적은 하지 않았다.
class NavGoalLogRecorder : public QObject
{
    Q_OBJECT

public:
    // issuedBy/userId: 로그인 세션의 표시용 아이디와 aurix_user.id(모르면 -1) —
    // user_action_log 기록에 그대로 쓴다.
    explicit NavGoalLogRecorder(
        RobotService &robotService,
        OperationsOverviewService &overviewService,
        const QString &issuedBy,
        int userId,
        QObject *parent = nullptr);

public slots:
    void handleSessionStarted(const QVector<NavPose> &goals, const QString &label);
    void handleWaypointProgress(int currentIndex, int total);
    void handleSessionFinished(bool success, bool cancelled);
    // nav_goal_log.start_x/y/yaw 채우는 용도로 최신 AMCL 위치를 캐싱만 한다
    // (첫 웨이포인트의 시작 위치로 씀 — 두 번째부터는 직전 목적지를 시작
    // 위치로 근사한다).
    void handleAmclPose(double x, double y, double yawRad);

private:
    RobotService &m_robotService;
    OperationsOverviewService &m_overviewService;
    QString m_issuedBy;
    int m_userId {-1};
    int m_activeSessionId {-1};
    bool m_hasAmclPose {false};
    double m_lastAmclX {0.0};
    double m_lastAmclY {0.0};
    double m_lastAmclYaw {0.0};
};

#endif // NAVGOALLOGRECORDER_H
