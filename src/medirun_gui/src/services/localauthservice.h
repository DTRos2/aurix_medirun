#ifndef LOCALAUTHSERVICE_H
#define LOCALAUTHSERVICE_H

#include "authservice.h"

class LocalAuthService : public AuthService
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

private:
    struct StoredUserRecord
    {
        QString loginId;
        QString passwordSalt;
        QString passwordHash;
        UserRole role {UserRole::Staff};
    };

    QVector<StoredUserRecord> loadUsers();
    void saveUsers(const QVector<StoredUserRecord> &users);
    void ensureDefaultUsers();
    QString hashPassword(const QString &password, const QString &salt) const;
};

#endif // LOCALAUTHSERVICE_H
