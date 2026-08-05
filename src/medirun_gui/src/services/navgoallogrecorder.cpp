#include "navgoallogrecorder.h"
#include "operationsoverviewservice.h"
#include "robotservice.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

NavGoalLogRecorder::NavGoalLogRecorder(
    RobotService &robotService, OperationsOverviewService &overviewService, const QString &issuedBy, int userId, QObject *parent)
    : QObject(parent)
    , m_robotService(robotService)
    , m_overviewService(overviewService)
    , m_issuedBy(issuedBy)
    , m_userId(userId)
{
}

void NavGoalLogRecorder::handleSessionStarted(const QVector<NavPose> &goals, const QString &label)
{
    m_activeSessionId = -1;

    const QVector<RobotRecord> robots = m_robotService.listRobots(false);
    if (robots.size() != 1) {
        return;
    }
    const int robotId = robots.first().id;

    QVector<NavGoalPoint> points;
    points.reserve(goals.size());
    for (const NavPose &goal : goals) {
        points.push_back({goal.x, goal.y, goal.yaw});
    }

    const QString sessionName = label.isEmpty()
        ? (goals.size() <= 1 ? QStringLiteral("웨이포인트 주행") : QStringLiteral("경로 주행 (%1개 웨이포인트)").arg(goals.size()))
        : (goals.size() <= 1 ? QStringLiteral("웨이포인트 주행: %1").arg(label) : QStringLiteral("경로 주행: %1").arg(label));

    const NavGoalPoint startPose {m_lastAmclX, m_lastAmclY, m_lastAmclYaw};

    int sessionId = -1;
    QString errorMessage;
    if (!m_overviewService.startRobotSession(
            robotId, QStringLiteral("auto"), sessionName, points, m_hasAmclPose, startPose, sessionId, errorMessage)) {
        qWarning() << "NavGoalLogRecorder::handleSessionStarted 실패:" << errorMessage;
        return;
    }
    m_activeSessionId = sessionId;

    QJsonObject payload;
    if (!label.isEmpty()) {
        payload.insert(QStringLiteral("label"), label);
    }
    payload.insert(QStringLiteral("goal_count"), goals.size());
    const QString payloadJson = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    QString actionErrorMessage;
    if (!m_overviewService.recordUserAction(
            robotId, sessionId, m_userId, QStringLiteral("waypoint_set_run"), payloadJson, m_issuedBy, actionErrorMessage)) {
        qWarning() << "NavGoalLogRecorder::handleSessionStarted (user_action_log) 실패:" << actionErrorMessage;
    }
}

void NavGoalLogRecorder::handleWaypointProgress(int currentIndex, int total)
{
    Q_UNUSED(total);
    if (m_activeSessionId < 0 || currentIndex <= 0) {
        return;
    }

    QString errorMessage;
    if (!m_overviewService.markGoalReached(m_activeSessionId, currentIndex - 1, errorMessage)) {
        qWarning() << "NavGoalLogRecorder::handleWaypointProgress 실패:" << errorMessage;
    }
}

void NavGoalLogRecorder::handleSessionFinished(bool success, bool cancelled)
{
    if (m_activeSessionId < 0) {
        return;
    }

    QString errorMessage;
    if (!m_overviewService.finishRobotSession(m_activeSessionId, success, cancelled, errorMessage)) {
        qWarning() << "NavGoalLogRecorder::handleSessionFinished 실패:" << errorMessage;
    }

    m_activeSessionId = -1;
}

void NavGoalLogRecorder::handleAmclPose(double x, double y, double yawRad)
{
    m_hasAmclPose = true;
    m_lastAmclX = x;
    m_lastAmclY = y;
    m_lastAmclYaw = yawRad;
}
