#include "dbconnection.h"

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>

namespace {

QString configPath()
{
    const QByteArray override = qgetenv("AURIX_DB_CONFIG");
    if (!override.isEmpty()) {
        return QString::fromLocal8Bit(override);
    }
    return QDir::current().filePath(QStringLiteral("config/db.yaml"));
}

} // namespace

bool openDbConnection(QString &errorMessage)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (db.isValid() && db.isOpen()) {
        errorMessage.clear();
        return true;
    }

    const QString path = configPath();
    if (!QFile::exists(path)) {
        errorMessage = QStringLiteral(
            "DB 설정 파일을 찾을 수 없습니다: %1 "
            "(config/db.yaml.example을 config/db.yaml로 복사하세요)").arg(path);
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
