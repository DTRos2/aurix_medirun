#ifndef DBROBOTSERVICE_H
#define DBROBOTSERVICE_H

#include "robotservice.h"

// PostgreSQL robot 테이블 기반 RobotService. 연결은 dbconnection.h(openDbConnection)를
// 통해 config/db.yaml 설정으로 연다.
class DbRobotService : public RobotService
{
public:
    QVector<RobotRecord> listRobots(bool includeDeregistered = false) override;
    bool registerRobot(const QString &name, const QString &model, QString &errorMessage) override;
    bool deregisterRobot(int robotId, QString &errorMessage) override;
};

#endif // DBROBOTSERVICE_H
