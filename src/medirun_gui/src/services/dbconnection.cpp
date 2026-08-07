#include "dbconnection.h"

#include <yaml-cpp/yaml.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>

namespace {

QString findConfigFrom(QDir dir)
{
    for (int i = 0; i < 10; ++i) {
        const QString candidate = dir.filePath(QStringLiteral("config/db.yaml"));
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

QString configPath(QString &attemptedPath)
{
    const QByteArray override = qgetenv("AURIX_DB_CONFIG");
    if (!override.isEmpty()) {
        attemptedPath = QString::fromLocal8Bit(override);
        return attemptedPath;
    }

    const QString currentDirResult = findConfigFrom(QDir::current());
    if (!currentDirResult.isEmpty()) {
        attemptedPath = currentDirResult;
        return currentDirResult;
    }

    const QString appDirResult = findConfigFrom(QDir(QCoreApplication::applicationDirPath()));
    if (!appDirResult.isEmpty()) {
        attemptedPath = appDirResult;
        return appDirResult;
    }

    attemptedPath = QDir::current().filePath(QStringLiteral("config/db.yaml"));
    return QString();
}

} // namespace

bool openDbConnection(QString &errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isValid() && db.isOpen()) {
        errorMessage.clear();
        return true;
    }

    QString attemptedPath;
    const QString path = configPath(attemptedPath);
    if (path.isEmpty()) {
        errorMessage = QStringLiteral(
            "DB 설정 파일을 찾을 수 없습니다: %1 "
            "(config/db.yaml.example을 config/db.yaml로 복사하세요)").arg(attemptedPath);
        return false;
    }

    YAML::Node root;
    try {
        root = YAML::LoadFile(path.toStdString());
    } catch (const std::exception &exc) {
        errorMessage = QStringLiteral("DB 설정 YAML을 읽지 못했습니다: %1").arg(QString::fromStdString(exc.what()));
        return false;
    }

    if (!root["host"] || !root["port"] || !root["database"] || !root["user"] || !root["password"]) {
        errorMessage = QStringLiteral("DB 설정 YAML에 host/port/database/user/password 항목이 모두 있어야 합니다.");
        return false;
    }

    if (!db.isValid()) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"));
    }
    db.setHostName(QString::fromStdString(root["host"].as<std::string>()));
    db.setPort(root["port"].as<int>());
    db.setDatabaseName(QString::fromStdString(root["database"].as<std::string>()));
    db.setUserName(QString::fromStdString(root["user"].as<std::string>()));
    db.setPassword(QString::fromStdString(root["password"].as<std::string>()));

    if (!db.open()) {
        errorMessage = QStringLiteral("DB 연결 실패: %1").arg(db.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}
