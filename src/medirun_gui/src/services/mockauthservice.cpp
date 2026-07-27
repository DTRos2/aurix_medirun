#include "mockauthservice.h"

bool MockAuthService::authenticate(
    const QString &username,
    const QString &password,
    UserRole requestedRole,
    AppSession &session,
    QString &errorMessage)
{
    const bool userMatch =
        (username == QStringLiteral("user") && password == QStringLiteral("user123"))
        || (username == QStringLiteral("staff") && password == QStringLiteral("staff"));
    const bool adminMatch =
        (username == QStringLiteral("admin") && password == QStringLiteral("admin123"))
        || (username == QStringLiteral("admin") && password == QStringLiteral("admin"));

    if (!userMatch && !adminMatch) {
        errorMessage = QStringLiteral("아이디 또는 비밀번호가 올바르지 않습니다.");
        return false;
    }

    if (userMatch && requestedRole == UserRole::Admin) {
        errorMessage = QStringLiteral("일반 사용자 계정으로는 ADMIN 권한 로그인이 불가합니다.");
        return false;
    }

    session.username = username;
    session.role = adminMatch ? UserRole::Admin : UserRole::Staff;
    session.mockMode = true;
    errorMessage.clear();
    return true;
}
