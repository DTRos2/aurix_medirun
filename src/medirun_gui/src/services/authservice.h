#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include "../app/appsession.h"

#include <QString>
#include <QVector>

struct AuthUserRecord
{
    QString loginId;
    UserRole role {UserRole::Staff};
};

class AuthService
{
public:
    virtual ~AuthService() = default;

    virtual bool authenticate(
        const QString &username,
        const QString &password,
        UserRole requestedRole,
        AppSession &session,
        QString &errorMessage) = 0;

    virtual QVector<AuthUserRecord> users() = 0;

    virtual bool createUser(
        const QString &loginId,
        const QString &password,
        UserRole role,
        QString &errorMessage) = 0;
};

#endif // AUTHSERVICE_H
