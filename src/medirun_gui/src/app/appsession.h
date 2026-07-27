#ifndef APPSESSION_H
#define APPSESSION_H

#include <QString>

enum class UserRole {
    Staff,
    Admin
};

struct AppSession
{
    QString username;
    UserRole role {UserRole::Staff};
    bool mockMode {true};
};

inline QString roleDisplayName(UserRole role)
{
    return role == UserRole::Admin ? QStringLiteral("ADMIN") : QStringLiteral("STAFF");
}

#endif // APPSESSION_H
