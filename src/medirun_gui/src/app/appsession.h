#ifndef APPSESSION_H
#define APPSESSION_H

#include <QString>

enum class UserRole {
    User,
    Admin
};

struct AppSession
{
    QString username;
    UserRole role {UserRole::User};
    bool mockMode {true};
};

inline QString roleDisplayName(UserRole role)
{
    return role == UserRole::Admin ? QStringLiteral("ADMIN") : QStringLiteral("USER");
}

#endif // APPSESSION_H
