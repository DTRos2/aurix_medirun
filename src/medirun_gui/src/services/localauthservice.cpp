#include "localauthservice.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSettings>

namespace {
QString settingsOrganization()
{
    return QStringLiteral("AURIX");
}

QString settingsApplication()
{
    return QStringLiteral("MediRunGui");
}

QString generateSalt()
{
    return QString::number(QRandomGenerator::global()->generate64(), 16);
}

QString normalizedLoginId(const QString &value)
{
    return value.trimmed().toLower();
}
}

bool LocalAuthService::authenticate(
    const QString &username,
    const QString &password,
    UserRole requestedRole,
    AppSession &session,
    QString &errorMessage)
{
    Q_UNUSED(requestedRole);
    ensureDefaultUsers();

    const QString loginId = normalizedLoginId(username);
    const QVector<StoredUserRecord> storedUsers = loadUsers();
    for (const StoredUserRecord &record : storedUsers) {
        if (record.loginId.compare(loginId, Qt::CaseInsensitive) != 0) {
            continue;
        }

        if (record.passwordHash != hashPassword(password, record.passwordSalt)) {
            errorMessage = QStringLiteral("아이디 또는 비밀번호가 올바르지 않습니다.");
            return false;
        }

        session.username = record.loginId;
        session.role = record.role;
        session.mockMode = false;
        errorMessage.clear();
        return true;
    }

    errorMessage = QStringLiteral("아이디 또는 비밀번호가 올바르지 않습니다.");
    return false;
}

QVector<AuthUserRecord> LocalAuthService::users()
{
    ensureDefaultUsers();

    QVector<AuthUserRecord> results;
    const QVector<StoredUserRecord> storedUsers = loadUsers();
    results.reserve(storedUsers.size());

    for (const StoredUserRecord &record : storedUsers) {
        results.push_back({record.loginId, record.role});
    }

    return results;
}

bool LocalAuthService::createUser(
    const QString &loginId,
    const QString &password,
    UserRole role,
    QString &errorMessage)
{
    ensureDefaultUsers();

    const QString normalizedId = normalizedLoginId(loginId);
    if (normalizedId.isEmpty()) {
        errorMessage = QStringLiteral("이메일 또는 아이디를 입력하세요.");
        return false;
    }

    if (password.size() < 8) {
        errorMessage = QStringLiteral("비밀번호는 8자 이상이어야 합니다.");
        return false;
    }

    QVector<StoredUserRecord> storedUsers = loadUsers();
    for (const StoredUserRecord &record : storedUsers) {
        if (record.loginId.compare(normalizedId, Qt::CaseInsensitive) == 0) {
            errorMessage = QStringLiteral("이미 등록된 이메일 또는 아이디입니다.");
            return false;
        }
    }

    const QString salt = generateSalt();
    storedUsers.push_back({normalizedId, salt, hashPassword(password, salt), role});
    saveUsers(storedUsers);
    errorMessage.clear();
    return true;
}

QVector<LocalAuthService::StoredUserRecord> LocalAuthService::loadUsers()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, settingsOrganization(), settingsApplication());
    const int size = settings.beginReadArray(QStringLiteral("users"));

    QVector<StoredUserRecord> users;
    users.reserve(size);
    for (int index = 0; index < size; ++index) {
        settings.setArrayIndex(index);
        StoredUserRecord record;
        record.loginId = normalizedLoginId(settings.value(QStringLiteral("loginId")).toString());
        record.passwordSalt = settings.value(QStringLiteral("passwordSalt")).toString();
        record.passwordHash = settings.value(QStringLiteral("passwordHash")).toString();
        const QString storedRole = settings.value(QStringLiteral("role")).toString().trimmed().toUpper();
        record.role = storedRole == QStringLiteral("ADMIN")
            ? UserRole::Admin
            : UserRole::User;

        if (!record.loginId.isEmpty() && !record.passwordSalt.isEmpty() && !record.passwordHash.isEmpty()) {
            users.push_back(record);
        }
    }
    settings.endArray();
    return users;
}

void LocalAuthService::saveUsers(const QVector<StoredUserRecord> &users)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, settingsOrganization(), settingsApplication());
    settings.remove(QStringLiteral("users"));
    settings.beginWriteArray(QStringLiteral("users"));
    for (int index = 0; index < users.size(); ++index) {
        settings.setArrayIndex(index);
        settings.setValue(QStringLiteral("loginId"), users.at(index).loginId);
        settings.setValue(QStringLiteral("passwordSalt"), users.at(index).passwordSalt);
        settings.setValue(QStringLiteral("passwordHash"), users.at(index).passwordHash);
        settings.setValue(QStringLiteral("role"), users.at(index).role == UserRole::Admin ? QStringLiteral("ADMIN") : QStringLiteral("USER"));
    }
    settings.endArray();
    settings.sync();
}

void LocalAuthService::ensureDefaultUsers()
{
    QVector<StoredUserRecord> storedUsers = loadUsers();
    bool dirty = false;

    const auto ensureUser = [&](const QString &loginId, const QString &password, UserRole role) {
        for (StoredUserRecord &record : storedUsers) {
            if (record.loginId.compare(loginId, Qt::CaseInsensitive) == 0) {
                if (record.role != role) {
                    record.role = role;
                    dirty = true;
                }
                return;
            }
        }

        const QString salt = generateSalt();
        storedUsers.push_back({loginId, salt, hashPassword(password, salt), role});
        dirty = true;
    };

    ensureUser(QStringLiteral("admin"), QStringLiteral("admin123"), UserRole::Admin);
    ensureUser(QStringLiteral("user"), QStringLiteral("user123"), UserRole::User);

    if (dirty) {
        saveUsers(storedUsers);
    }
}

QString LocalAuthService::hashPassword(const QString &password, const QString &salt) const
{
    return QString::fromUtf8(
        QCryptographicHash::hash((salt + password).toUtf8(), QCryptographicHash::Sha256).toHex());
}
