#include "waypointfile.h"

#include <yaml-cpp/yaml.h>

#include <QFile>
#include <QTextStream>

namespace {

QString kindToString(WaypointMarkerKind kind)
{
    switch (kind) {
    case WaypointMarkerKind::CallZone:
        return QStringLiteral("call_zone");
    case WaypointMarkerKind::Home:
        return QStringLiteral("home");
    case WaypointMarkerKind::Waypoint:
        break;
    }
    return QStringLiteral("waypoint");
}

WaypointMarkerKind kindFromString(const std::string &text)
{
    if (text == "call_zone") {
        return WaypointMarkerKind::CallZone;
    }
    if (text == "home") {
        return WaypointMarkerKind::Home;
    }
    return WaypointMarkerKind::Waypoint;
}

} // namespace

bool loadWaypointYaml(const QString &path,
                      QString &frameId,
                      WaypointMap &waypoints,
                      TrajectoryMap &trajectories,
                      QString &errorMessage)
{
    frameId = QStringLiteral("map");
    waypoints.clear();
    trajectories.clear();

    YAML::Node root;
    try {
        root = YAML::LoadFile(path.toStdString());
    } catch (const std::exception &exc) {
        errorMessage = QStringLiteral("waypoint 파일을 읽지 못했습니다: %1").arg(QString::fromStdString(exc.what()));
        return false;
    }

    if (!root.IsMap()) {
        errorMessage = QStringLiteral("YAML 최상위는 매핑(딕셔너리)이어야 합니다.");
        return false;
    }

    if (root["frame_id"]) {
        frameId = QString::fromStdString(root["frame_id"].as<std::string>());
    }

    const YAML::Node rawWaypoints = root["waypoints"];
    if (!rawWaypoints || !rawWaypoints.IsMap()) {
        errorMessage = QStringLiteral("'waypoints' 항목은 이름:좌표 매핑이어야 합니다.");
        return false;
    }

    for (const auto &entry : rawWaypoints) {
        const QString name = QString::fromStdString(entry.first.as<std::string>());
        const YAML::Node value = entry.second;
        if (!value.IsMap() || !value["x"] || !value["y"]) {
            errorMessage = QStringLiteral("waypoint '%1' 형식 오류: x/y 필드가 필요합니다.").arg(name);
            return false;
        }

        WaypointDef def;
        def.name = name;
        try {
            def.x = value["x"].as<double>();
            def.y = value["y"].as<double>();
            def.yaw = value["yaw"] ? value["yaw"].as<double>() : 0.0;
        } catch (const std::exception &exc) {
            errorMessage = QStringLiteral("waypoint '%1' 형식 오류: %2").arg(name, QString::fromStdString(exc.what()));
            return false;
        }
        def.kind = value["kind"] ? kindFromString(value["kind"].as<std::string>()) : WaypointMarkerKind::Waypoint;
        def.label = value["label"] ? QString::fromStdString(value["label"].as<std::string>()) : name;
        waypoints.insert(name, def);
    }

    if (waypoints.isEmpty()) {
        errorMessage = QStringLiteral("정의된 waypoint가 없습니다.");
        return false;
    }

    const YAML::Node rawTrajectories = root["trajectories"];
    if (rawTrajectories && !rawTrajectories.IsNull()) {
        if (!rawTrajectories.IsMap()) {
            errorMessage = QStringLiteral("'trajectories' 항목은 이름:웨이포인트 목록 매핑이어야 합니다.");
            return false;
        }
        for (const auto &entry : rawTrajectories) {
            const QString name = QString::fromStdString(entry.first.as<std::string>());
            const YAML::Node seq = entry.second;
            if (!seq.IsSequence() || seq.size() == 0) {
                errorMessage = QStringLiteral("trajectory '%1'은(는) 비어있지 않은 목록이어야 합니다.").arg(name);
                return false;
            }
            QStringList names;
            for (const auto &item : seq) {
                const QString wpName = QString::fromStdString(item.as<std::string>());
                if (!waypoints.contains(wpName)) {
                    errorMessage = QStringLiteral("trajectory '%1'이(가) 존재하지 않는 waypoint를 참조합니다: %2")
                                       .arg(name, wpName);
                    return false;
                }
                names.append(wpName);
            }
            trajectories.insert(name, names);
        }
    }

    return true;
}

bool saveWaypointYaml(const QString &path,
                      const QString &frameId,
                      const WaypointMap &waypoints,
                      const TrajectoryMap &trajectories,
                      QString &errorMessage)
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "frame_id" << YAML::Value << frameId.toStdString();

    out << YAML::Key << "waypoints" << YAML::Value << YAML::BeginMap;
    for (auto it = waypoints.cbegin(); it != waypoints.cend(); ++it) {
        const WaypointDef &def = it.value();
        out << YAML::Key << it.key().toStdString() << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "x" << YAML::Value << def.x;
        out << YAML::Key << "y" << YAML::Value << def.y;
        out << YAML::Key << "yaw" << YAML::Value << def.yaw;
        out << YAML::Key << "kind" << YAML::Value << kindToString(def.kind).toStdString();
        out << YAML::Key << "label" << YAML::Value << (def.label.isEmpty() ? it.key() : def.label).toStdString();
        out << YAML::EndMap;
    }
    out << YAML::EndMap;

    out << YAML::Key << "trajectories" << YAML::Value << YAML::BeginMap;
    for (auto it = trajectories.cbegin(); it != trajectories.cend(); ++it) {
        out << YAML::Key << it.key().toStdString() << YAML::Value << YAML::BeginSeq;
        for (const QString &name : it.value()) {
            out << name.toStdString();
        }
        out << YAML::EndSeq;
    }
    out << YAML::EndMap;

    out << YAML::EndMap;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("waypoint 파일을 저장할 수 없습니다: %1").arg(path);
        return false;
    }
    QTextStream stream(&file);
    stream << out.c_str();
    return true;
}
