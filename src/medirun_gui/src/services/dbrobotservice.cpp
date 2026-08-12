#include "dbrobotservice.h"
#include "dbconnection.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

QVector<RobotRecord> DbRobotService::listRobots(bool includeDeregistered)
{
    QVector<RobotRecord> results;

    QString errorMessage;
    if (!openDbConnection(errorMessage)) {
        return results;
    }

    QSqlQuery query(includeDeregistered
            ? QStringLiteral("SELECT id, name, model, created_at, deregistered_at FROM robot ORDER BY name")
            : QStringLiteral(
                  "SELECT id, name, model, created_at, deregistered_at FROM robot "
                  "WHERE deregistered_at IS NULL ORDER BY name"));

    while (query.next()) {
        RobotRecord record;
        record.id = query.value(QStringLiteral("id")).toInt();
        record.name = query.value(QStringLiteral("name")).toString();
        record.model = query.value(QStringLiteral("model")).toString();
        record.createdAt = dbTimestampToLocal(query.value(QStringLiteral("created_at")));
        record.active = query.value(QStringLiteral("deregistered_at")).isNull();
        results.push_back(record);
    }

    return results;
}

bool DbRobotService::registerRobot(const QString &name, const QString &model, QString &errorMessage)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        errorMessage = QStringLiteral("로봇 이름을 입력하세요.");
        return false;
    }

    const QString trimmedModel = model.trimmed().isEmpty() ? QStringLiteral("turtlebot3_burger") : model.trimmed();

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    // 같은 로봇을 여러 세션/모드로 중복 등록할 수 있어야 해서 이름 중복은
    // 막지 않는다(id로만 구분). robot 테이블 목록은 실제 로봇 대수와 무관한
    // "등록 이력"에 가까워짐.
    QSqlQuery insertQuery;
    insertQuery.prepare(QStringLiteral("INSERT INTO robot (name, model) VALUES (:name, :model)"));
    insertQuery.bindValue(QStringLiteral(":name"), trimmedName);
    insertQuery.bindValue(QStringLiteral(":model"), trimmedModel);
    if (!insertQuery.exec()) {
        qWarning() << "DbRobotService::registerRobot INSERT 실패:" << insertQuery.lastError().text();
        errorMessage = QStringLiteral("로봇 등록에 실패했습니다. 잠시 후 다시 시도해주세요.");
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbRobotService::deregisterRobot(int robotId, QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "UPDATE robot SET deregistered_at = NOW() WHERE id = :id AND deregistered_at IS NULL"));
    query.bindValue(QStringLiteral(":id"), robotId);
    if (!query.exec()) {
        qWarning() << "DbRobotService::deregisterRobot UPDATE 실패:" << query.lastError().text();
        errorMessage = QStringLiteral("로봇 등록 해제에 실패했습니다. 잠시 후 다시 시도해주세요.");
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        errorMessage = QStringLiteral("해당 로봇을 찾을 수 없습니다.");
        return false;
    }

    errorMessage.clear();
    return true;
}
