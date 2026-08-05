#ifndef ROBOTSERVICE_H
#define ROBOTSERVICE_H

#include <QDateTime>
#include <QString>
#include <QVector>

// robot 테이블(db/schema/postgresql_schema.sql) 등록 정보 CRUD.
// AuthService/DbAuthService와 동일한 패턴으로 DbRobotService가 실 구현체다.

struct RobotRecord
{
    int id {0};
    QString name;
    QString model;
    QDateTime createdAt;
    bool active {true}; // deregistered_at IS NULL
};

class RobotService
{
public:
    virtual ~RobotService() = default;

    virtual QVector<RobotRecord> listRobots(bool includeDeregistered = false) = 0;

    virtual bool registerRobot(const QString &name, const QString &model, QString &errorMessage) = 0;

    virtual bool deregisterRobot(int robotId, QString &errorMessage) = 0;
};

#endif // ROBOTSERVICE_H
