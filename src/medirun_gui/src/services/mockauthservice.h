#ifndef MOCKAUTHSERVICE_H
#define MOCKAUTHSERVICE_H

#include "authservice.h"

class MockAuthService : public AuthService
{
public:
    bool authenticate(
        const QString &username,
        const QString &password,
        UserRole requestedRole,
        AppSession &session,
        QString &errorMessage) override;
};

#endif // MOCKAUTHSERVICE_H
