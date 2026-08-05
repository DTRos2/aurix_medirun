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
    // aurix_user.id — DB로 인증한 세션만 채워진다. mock/local 인증이나 아직
    // 로그인 전이면 -1(알 수 없음 — user_action_log.user_id는 이때 NULL로 남김).
    int userId {-1};
};

inline QString roleDisplayName(UserRole role)
{
    return role == UserRole::Admin ? QStringLiteral("ADMIN") : QStringLiteral("STAFF");
}

#endif // APPSESSION_H
