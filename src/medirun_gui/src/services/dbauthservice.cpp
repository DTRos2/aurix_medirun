#include "dbauthservice.h"
#include "dbconnection.h"

#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>

#include <cstring>
#include <crypt.h>

namespace {

QString normalizedLoginId(const QString &value)
{
    return value.trimmed().toLower();
}

// glibc/libxcrypt $6$ (SHA-512) 형식 salt. aurix_user.password_hash VARCHAR(255)에 여유 있게 들어간다.
QString generateCryptSalt()
{
    static const QString alphabet = QStringLiteral(
        "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    QString salt = QStringLiteral("$6$");
    for (int i = 0; i < 16; ++i) {
        salt += alphabet.at(QRandomGenerator::global()->bounded(alphabet.size()));
    }
    salt += QStringLiteral("$");
    return salt;
}

QString hashPassword(const QString &password, const QString &salt)
{
    struct crypt_data data;
    std::memset(&data, 0, sizeof(data));

    const char *result = crypt_r(
        password.toUtf8().constData(), salt.toUtf8().constData(), &data);
    return result != nullptr ? QString::fromUtf8(result) : QString();
}

QString roleToDbValue(UserRole role)
{
    return role == UserRole::Admin ? QStringLiteral("admin") : QStringLiteral("staff");
}

UserRole roleFromDbValue(const QString &value)
{
    return value.compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0
        ? UserRole::Admin
        : UserRole::Staff;
}

} // namespace

bool DbAuthService::authenticate(
    const QString &username,
    const QString &password,
    UserRole requestedRole,
    AppSession &session,
    QString &errorMessage)
{
    Q_UNUSED(requestedRole);

    if (!openDbConnection(errorMessage)) {
        return false;
    }
    if (!ensureDefaultUsers(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "SELECT id, login_id, password_hash, role FROM aurix_user "
        "WHERE lower(login_id) = :loginId AND deleted_at IS NULL"));
    query.bindValue(QStringLiteral(":loginId"), normalizedLoginId(username));

    if (!query.exec()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(query.lastError().text());
        return false;
    }

    if (!query.next()) {
        errorMessage = QStringLiteral("아이디 또는 비밀번호가 올바르지 않습니다.");
        return false;
    }

    const int userId = query.value(QStringLiteral("id")).toInt();
    const QString storedLoginId = query.value(QStringLiteral("login_id")).toString();
    const QString storedHash = query.value(QStringLiteral("password_hash")).toString();
    const UserRole role = roleFromDbValue(query.value(QStringLiteral("role")).toString());

    if (storedHash.isEmpty() || hashPassword(password, storedHash) != storedHash) {
        errorMessage = QStringLiteral("아이디 또는 비밀번호가 올바르지 않습니다.");
        return false;
    }

    QSqlQuery touchQuery;
    touchQuery.prepare(QStringLiteral("UPDATE aurix_user SET last_login_at = NOW() WHERE id = :id"));
    touchQuery.bindValue(QStringLiteral(":id"), userId);
    touchQuery.exec();

    session.username = storedLoginId;
    session.role = role;
    session.mockMode = false;
    errorMessage.clear();
    return true;
}

QVector<AuthUserRecord> DbAuthService::users()
{
    QVector<AuthUserRecord> results;

    QString errorMessage;
    if (!openDbConnection(errorMessage) || !ensureDefaultUsers(errorMessage)) {
        return results;
    }

    QSqlQuery query(QStringLiteral(
        "SELECT login_id, role FROM aurix_user WHERE deleted_at IS NULL ORDER BY login_id"));
    while (query.next()) {
        results.push_back({
            query.value(QStringLiteral("login_id")).toString(),
            roleFromDbValue(query.value(QStringLiteral("role")).toString())
        });
    }

    return results;
}

bool DbAuthService::createUser(
    const QString &loginId,
    const QString &password,
    UserRole role,
    QString &errorMessage)
{
    const QString normalizedId = normalizedLoginId(loginId);
    if (normalizedId.isEmpty()) {
        errorMessage = QStringLiteral("이메일 또는 아이디를 입력하세요.");
        return false;
    }

    if (password.size() < 8) {
        errorMessage = QStringLiteral("비밀번호는 8자 이상이어야 합니다.");
        return false;
    }

    if (!openDbConnection(errorMessage)) {
        return false;
    }
    if (!ensureDefaultUsers(errorMessage)) {
        return false;
    }

    // 삭제된(deleted_at IS NOT NULL) 계정의 아이디는 재사용 가능하므로 활성 계정끼리만 중복 확인한다.
    QSqlQuery checkQuery;
    checkQuery.prepare(QStringLiteral(
        "SELECT 1 FROM aurix_user WHERE lower(login_id) = :loginId AND deleted_at IS NULL"));
    checkQuery.bindValue(QStringLiteral(":loginId"), normalizedId);
    if (!checkQuery.exec()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(checkQuery.lastError().text());
        return false;
    }
    if (checkQuery.next()) {
        errorMessage = QStringLiteral("이미 사용 중인 아이디입니다. 동일한 아이디로는 계정을 생성할 수 없습니다.");
        return false;
    }

    return insertUser(normalizedId, password, role, errorMessage);
}

bool DbAuthService::deleteUser(const QString &loginId, QString &errorMessage)
{
    const QString normalizedId = normalizedLoginId(loginId);
    if (normalizedId.isEmpty()) {
        errorMessage = QStringLiteral("이메일 또는 아이디를 입력하세요.");
        return false;
    }

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery deleteQuery;
    deleteQuery.prepare(QStringLiteral(
        "UPDATE aurix_user SET deleted_at = NOW() "
        "WHERE lower(login_id) = :loginId AND deleted_at IS NULL"));
    deleteQuery.bindValue(QStringLiteral(":loginId"), normalizedId);

    if (!deleteQuery.exec()) {
        errorMessage = QStringLiteral("계정 삭제 실패: %1").arg(deleteQuery.lastError().text());
        return false;
    }

    if (deleteQuery.numRowsAffected() <= 0) {
        errorMessage = QStringLiteral("해당 계정을 찾을 수 없습니다.");
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbAuthService::insertUser(const QString &normalizedLoginId, const QString &password, UserRole role, QString &errorMessage)
{
    QSqlQuery insertQuery;
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO aurix_user (login_id, password_hash, role) VALUES (:loginId, :passwordHash, :role)"));
    insertQuery.bindValue(QStringLiteral(":loginId"), normalizedLoginId);
    insertQuery.bindValue(QStringLiteral(":passwordHash"), hashPassword(password, generateCryptSalt()));
    insertQuery.bindValue(QStringLiteral(":role"), roleToDbValue(role));

    if (!insertQuery.exec()) {
        errorMessage = QStringLiteral("사용자 생성 실패: %1").arg(insertQuery.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbAuthService::ensureDefaultUsers(QString &errorMessage)
{
    if (m_defaultsEnsured) {
        return true;
    }

    QSqlQuery countQuery(QStringLiteral("SELECT COUNT(*) FROM aurix_user WHERE deleted_at IS NULL"));
    if (!countQuery.isActive() || !countQuery.next()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(countQuery.lastError().text());
        return false;
    }

    if (countQuery.value(0).toInt() == 0) {
        QString insertError;
        if (!insertUser(QStringLiteral("admin"), QStringLiteral("admin123"), UserRole::Admin, insertError)
            || !insertUser(QStringLiteral("user"), QStringLiteral("user123"), UserRole::Staff, insertError)) {
            errorMessage = QStringLiteral("기본 계정 생성 실패: %1").arg(insertError);
            return false;
        }
    }

    m_defaultsEnsured = true;
    return true;
}
