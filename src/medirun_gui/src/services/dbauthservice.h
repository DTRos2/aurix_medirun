#ifndef DBAUTHSERVICE_H
#define DBAUTHSERVICE_H

#include "authservice.h"

// PostgreSQL aurix_user 테이블(db/schema/postgresql_schema.sql) 기반 AuthService.
// 연결은 dbconnection.h(openDbConnection)를 통해 config/db.yaml 설정으로 연다.
class DbAuthService : public AuthService
{
public:
    bool authenticate(
        const QString &username,
        const QString &password,
        UserRole requestedRole,
        AppSession &session,
        QString &errorMessage) override;

    QVector<AuthUserRecord> users() override;

    bool createUser(
        const QString &loginId,
        const QString &password,
        UserRole role,
        QString &errorMessage) override;

    bool deleteUser(const QString &loginId, QString &errorMessage) override;

private:
    bool ensureDefaultUsers(QString &errorMessage);
    bool insertUser(const QString &normalizedLoginId, const QString &password, UserRole role, QString &errorMessage);

    bool m_defaultsEnsured {false};
};

#endif // DBAUTHSERVICE_H
