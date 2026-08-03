#include "slammappingpage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/mapviewwidget.h"
#include "../widgets/proportionalbar.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFrame>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <thread>

#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/subscription_options.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <tf2_msgs/msg/tf_message.hpp>
#include <tf2/exceptions.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

namespace {
QLabel *makeLabel(const QString &text, const QString &objectName = {})
{
    auto *label = new QLabel(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    label->setWordWrap(true);
    return label;
}

QFrame *panelFrame(const QString &objectName)
{
    auto *frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

QHBoxLayout *statRow(QBoxLayout *parentLayout, const QString &label, const QString &value, const QColor &valueColor = DashboardPalette::TextPrimary)
{
    auto *row = new QHBoxLayout;
    row->addWidget(makeLabel(label, QStringLiteral("metricCaption")));
    row->addStretch();
    auto *valueLabel = makeLabel(value);
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(valueColor.name()));
    row->addWidget(valueLabel);
    parentLayout->addLayout(row);
    return row;
}

QPushButton *directionButton(const QString &text, bool danger = false)
{
    auto *button = new QPushButton(text);
    button->setObjectName(danger ? QStringLiteral("dangerFilledButton") : QStringLiteral("directionPadButton"));
    button->setFixedSize(38, 38);
    return button;
}

QImage loadPgmImage(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QImage();
    }

    auto skipWhitespaceAndComments = [&file]() {
        while (!file.atEnd()) {
            char c = 0;
            if (!file.getChar(&c)) {
                break;
            }
            if (c == '#') {
                file.readLine();
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                continue;
            }
            file.ungetChar(c);
            break;
        }
    };

    auto readToken = [&file, &skipWhitespaceAndComments]() {
        skipWhitespaceAndComments();
        QByteArray token;
        char c = 0;
        while (file.getChar(&c)) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
            token.append(c);
        }
        return token;
    };

    if (readToken() != QByteArrayLiteral("P5")) {
        return QImage();
    }

    bool widthOk = false;
    bool heightOk = false;
    bool maxValOk = false;
    const int width = readToken().toInt(&widthOk);
    const int height = readToken().toInt(&heightOk);
    const int maxVal = readToken().toInt(&maxValOk);
    if (!widthOk || !heightOk || !maxValOk || width <= 0 || height <= 0 || maxVal <= 0 || maxVal > 255) {
        return QImage();
    }

    const QByteArray raster = file.read(static_cast<qint64>(width) * height);
    if (raster.size() != width * height) {
        return QImage();
    }

    QImage image(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        std::memcpy(image.scanLine(y), raster.constData() + y * width, width);
    }
    return image;
}

QString rewriteYamlImagePath(const QString &yamlPath, const QString &pngFileName)
{
    QFile yamlFile(yamlPath);
    if (!yamlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("YAML 파일을 열 수 없습니다.");
    }

    const QString original = QString::fromUtf8(yamlFile.readAll());
    yamlFile.close();

    const QStringList lines = original.split(QLatin1Char('\n'));
    QStringList rewritten;
    rewritten.reserve(lines.size());
    bool replaced = false;
    for (const QString &line : lines) {
        if (!replaced && line.trimmed().startsWith(QStringLiteral("image:"))) {
            rewritten.append(QStringLiteral("image: %1").arg(pngFileName));
            replaced = true;
        } else {
            rewritten.append(line);
        }
    }

    if (!replaced) {
        return QStringLiteral("YAML 파일에서 image 항목을 찾지 못했습니다.");
    }

    if (!yamlFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return QStringLiteral("YAML 파일을 다시 저장할 수 없습니다.");
    }
    yamlFile.write(rewritten.join(QLatin1Char('\n')).toUtf8());
    yamlFile.close();
    return QString();
}

QString findWorkspaceRoot()
{
    const QStringList candidateRoots = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath()
    };

    for (const QString &candidate : candidateRoots) {
        QDir dir(candidate);
        for (int depth = 0; depth < 8; ++depth) {
            if (dir.exists(QStringLiteral("maps")) && dir.exists(QStringLiteral("src/medirun_gui"))) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }
    return QString();
}

QString shellCommandPrefix()
{
    QStringList commands;
    if (QFile::exists(QStringLiteral("/opt/ros/humble/setup.bash"))) {
        commands.append(QStringLiteral("source /opt/ros/humble/setup.bash"));
    }

    const QString turtlebotWorkspaceSetup = QDir::home().filePath(QStringLiteral("turtlebot3_ws/install/setup.bash"));
    if (QFile::exists(turtlebotWorkspaceSetup)) {
        commands.append(QStringLiteral("source \"%1\"").arg(turtlebotWorkspaceSetup));
    }

    const QString workspaceRoot = findWorkspaceRoot();
    if (!workspaceRoot.isEmpty()) {
        const QString installSetup = QDir(workspaceRoot).filePath(QStringLiteral("install/setup.bash"));
        if (QFile::exists(installSetup)) {
            commands.append(QStringLiteral("source \"%1\"").arg(installSetup));
        }
    }

    if (commands.isEmpty()) {
        return QString();
    }

    return commands.join(QStringLiteral(" && ")) + QStringLiteral(" && ");
}

constexpr double kManualLinearSpeedMps = 0.06;
constexpr double kManualAngularSpeedRadPerSec = 0.20;
constexpr int kMapRefreshIntervalMs = 300;
constexpr int kMapTimeoutMs = 10000;
constexpr auto kRecentSensorTimeout = std::chrono::seconds(3);
constexpr auto kRecentTfTimeout = std::chrono::seconds(5);

struct RobotPoseSnapshot
{
    bool valid {false};
    QPointF normalizedPos {0.5, 0.5};
    qreal headingDegrees {0.0};
};

enum class TopicHealth {
    Unavailable,
    Waiting,
    Ready,
    Stale,
};

struct TopicStateSnapshot
{
    TopicHealth scanHealth {TopicHealth::Unavailable};
    TopicHealth odomHealth {TopicHealth::Unavailable};
    TopicHealth tfHealth {TopicHealth::Unavailable};
    TopicHealth tfStaticHealth {TopicHealth::Unavailable};
    size_t scanPublisherCount {0};
    size_t odomPublisherCount {0};
    size_t tfPublisherCount {0};
    size_t tfStaticPublisherCount {0};
    bool scanReceived {false};
    bool odomReceived {false};
    bool tfReceived {false};
    bool tfStaticReceived {false};
    bool scanRecent {false};
    bool odomRecent {false};
    bool tfRecent {false};
    bool tfReady {false};
    bool odomUsable {false};
    bool readyForMapping {false};
    uint64_t odomMessageCount {0};
    double odomLastAgeSec {-1.0};
    QString odomFrameId;
    QString odomChildFrameId;
};

QString topicHealthText(TopicHealth health)
{
    switch (health) {
    case TopicHealth::Unavailable:
        return QStringLiteral("없음");
    case TopicHealth::Waiting:
        return QStringLiteral("대기");
    case TopicHealth::Ready:
        return QStringLiteral("OK");
    case TopicHealth::Stale:
        return QStringLiteral("경고");
    }
    return QStringLiteral("알 수 없음");
}

QString readinessFailureMessage(const TopicStateSnapshot &snapshot)
{
    if (snapshot.scanPublisherCount == 0) {
        return QStringLiteral("/scan publisher가 존재하지 않습니다.");
    }
    if (!snapshot.scanReceived || !snapshot.scanRecent) {
        return QStringLiteral("LiDAR 데이터가 없습니다: /scan 확인 필요");
    }
    if (snapshot.odomPublisherCount == 0) {
        return QStringLiteral("/odom publisher가 존재하지 않습니다.");
    }
    if (!snapshot.odomReceived) {
        return QStringLiteral("odom 메시지를 아직 수신하지 못했습니다: /odom 확인 필요");
    }
    if (!snapshot.tfReady) {
        return QStringLiteral("TF 데이터가 없습니다: /tf 또는 /tf_static 확인 필요");
    }
    return QStringLiteral("Bringup 준비 상태를 다시 확인하세요.");
}

bool createOccupancyGridImage(const nav_msgs::msg::OccupancyGrid &map, QImage *imageOut)
{
    static bool hasLoggedImageCreated = false;

    if (imageOut == nullptr) {
        return false;
    }

    const int width = static_cast<int>(map.info.width);
    const int height = static_cast<int>(map.info.height);
    const size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (width <= 0 || height <= 0) {
        qWarning() << "Invalid map dimensions:" << width << "x" << height;
        return false;
    }
    if (map.data.size() != expectedSize) {
        qWarning() << "Invalid map data size:" << map.data.size() << "expected:" << expectedSize;
        return false;
    }

    QImage image(width, height, QImage::Format_RGB32);
    if (image.isNull()) {
        qWarning() << "Failed to allocate map image:" << width << "x" << height;
        return false;
    }
    image.fill(Qt::white);

    for (int y = 0; y < height; ++y) {
        const int imageY = height - 1 - y;
        QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(imageY));
        for (int x = 0; x < width; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                + static_cast<std::size_t>(x);
            const int value = static_cast<int>(map.data.at(index));

            QRgb color = qRgb(255, 255, 255);
            if (value < 0) {
                color = qRgb(255, 255, 255);
            } else if (value == 0) {
                color = qRgb(250, 250, 250);
            } else if (value >= 65) {
                color = qRgb(25, 30, 35);
            } else {
                const int gray = std::clamp(
                    245 - (value * 180 / 64),
                    65,
                    245);
                color = qRgb(gray, gray, gray);
            }

            scanLine[x] = color;
        }
    }

    *imageOut = image;
    if (!hasLoggedImageCreated) {
        qInfo() << "[MapView] map image created:" << width << "x" << height;
        hasLoggedImageCreated = true;
    }
    return true;
}

} // namespace

class SlamMapBridge
{
public:
    SlamMapBridge()
        : m_node(std::make_shared<rclcpp::Node>("medirun_gui_slam_map_bridge"))
        , m_executor(std::make_unique<rclcpp::executors::SingleThreadedExecutor>())
        , m_running(false)
        , m_mapReceived(false)
    {
        m_cmdVelPublisher = m_node->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", rclcpp::QoS(10));
        m_tfBuffer = std::make_unique<tf2_ros::Buffer>(m_node->get_clock());
        m_tfListener = std::make_shared<tf2_ros::TransformListener>(*m_tfBuffer);

        rclcpp::SubscriptionOptions mapOptions;
        mapOptions.event_callbacks.incompatible_qos_callback =
            [this](rclcpp::QOSRequestedIncompatibleQoSInfo &info) {
                RCLCPP_ERROR(
                    m_node->get_logger(),
                    "[SLAM] /map QoS incompatible: policy=%d total=%d",
                    info.last_policy_kind,
                    info.total_count);
                if (onStatusUpdated) {
                    onStatusUpdated(QStringLiteral("/map QoS가 publisher와 맞지 않습니다."));
                }
            };

        const auto mapQos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
        m_mapSubscription = m_node->create_subscription<nav_msgs::msg::OccupancyGrid>(
            "/map",
            mapQos,
            [this](const nav_msgs::msg::OccupancyGrid::ConstSharedPtr msg) { handleMap(msg); },
            mapOptions);
        m_odomSubscription = m_node->create_subscription<nav_msgs::msg::Odometry>(
            "/odom",
            rclcpp::SystemDefaultsQoS(),
            [this](const nav_msgs::msg::Odometry::ConstSharedPtr msg) { handleOdom(msg); });
        m_scanSubscription = m_node->create_subscription<sensor_msgs::msg::LaserScan>(
            "/scan",
            rclcpp::SensorDataQoS(),
            [this](const sensor_msgs::msg::LaserScan::ConstSharedPtr msg) { handleScan(msg); });
        m_tfSubscription = m_node->create_subscription<tf2_msgs::msg::TFMessage>(
            "/tf",
            rclcpp::SystemDefaultsQoS(),
            [this](const tf2_msgs::msg::TFMessage::ConstSharedPtr msg) { handleTf(msg, false); });
        m_tfStaticSubscription = m_node->create_subscription<tf2_msgs::msg::TFMessage>(
            "/tf_static",
            rclcpp::QoS(rclcpp::KeepLast(1)).transient_local().reliable(),
            [this](const tf2_msgs::msg::TFMessage::ConstSharedPtr msg) { handleTf(msg, true); });
        resetMapSessionTracking();
        resetTopicTracking();
    }

    ~SlamMapBridge()
    {
        stop();
    }

    void start()
    {
        if (m_running.exchange(true)) {
            return;
        }
        m_executor->add_node(m_node);
        m_spinThread = std::thread([this]() { m_executor->spin(); });
        if (onStatusUpdated) {
            onStatusUpdated(QStringLiteral("Bringup 토픽 구독 대기 중 (/scan, /odom, /tf, /tf_static, /map)"));
        }
    }

    void stop()
    {
        if (!m_running.exchange(false)) {
            return;
        }
        m_executor->cancel();
        if (m_spinThread.joinable()) {
            m_spinThread.join();
        }
        m_executor->remove_node(m_node);
    }

    void resetMapSessionTracking()
    {
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_latestMap.reset();
            m_mapDirty = false;
            m_latestRobotPose = RobotPoseSnapshot();
        }
        m_mapReceived = false;
        m_hasInitialOdom = false;
        m_hasMapAnchor = false;
        m_mapWidth = 0;
        m_mapHeight = 0;
        m_mapResolution = 0.0;
        m_mapOriginX = 0.0;
        m_mapOriginY = 0.0;
        m_hasPublisherInfoLogged = false;
        m_hasLoggedFirstAcceptedMap = false;
        m_hasLoggedMapInfo = false;
        m_lastLoggedWidth = 0;
        m_lastLoggedHeight = 0;
        m_lastLoggedResolution = 0.0;
        m_lastLoggedOriginX = 0.0;
        m_lastLoggedOriginY = 0.0;
        m_lastLoggedDataSize = 0;
        m_lastInvalidWidth = -1;
        m_lastInvalidHeight = -1;
        m_lastInvalidDataSize = 0;
    }

    void resetTopicTracking()
    {
        m_hasScan = false;
        m_hasTf = false;
        m_hasTfStatic = false;
        m_hasLatestOdom = false;
        m_odomMessageCount = 0;
        m_lastScanAt = {};
        m_lastOdomAt = {};
        m_lastTfAt = {};
        m_lastTfStaticAt = {};
        m_lastOdomFrameId.clear();
        m_lastOdomChildFrameId.clear();
    }

    void beginFreshSession()
    {
        resetMapSessionTracking();
    }

    std::function<void(const QString &)> onStatusUpdated;

    void publishVelocity(double linearX, double angularZ)
    {
        geometry_msgs::msg::Twist cmd;
        cmd.linear.x = linearX;
        cmd.angular.z = angularZ;
        m_cmdVelPublisher->publish(cmd);
    }

    nav_msgs::msg::OccupancyGrid::ConstSharedPtr peekLatestMap()
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        if (!m_mapDirty || !m_latestMap) {
            return nullptr;
        }
        return m_latestMap;
    }

    void markLatestMapDisplayed()
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_mapDirty = false;
    }

    RobotPoseSnapshot takeLatestRobotPose()
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        return m_latestRobotPose;
    }

    TopicStateSnapshot topicStateSnapshot() const
    {
        const auto now = std::chrono::steady_clock::now();
        TopicStateSnapshot snapshot;
        snapshot.scanPublisherCount = m_node->count_publishers("/scan");
        snapshot.odomPublisherCount = m_node->count_publishers("/odom");
        snapshot.tfPublisherCount = m_node->count_publishers("/tf");
        snapshot.tfStaticPublisherCount = m_node->count_publishers("/tf_static");
        snapshot.scanReceived = m_hasScan;
        snapshot.odomReceived = m_hasLatestOdom;
        snapshot.tfReceived = m_hasTf;
        snapshot.tfStaticReceived = m_hasTfStatic;
        snapshot.scanRecent = m_hasScan && (now - m_lastScanAt <= kRecentSensorTimeout);
        snapshot.odomRecent = m_hasLatestOdom && (now - m_lastOdomAt <= kRecentSensorTimeout);
        snapshot.tfRecent = m_hasTf && (now - m_lastTfAt <= kRecentTfTimeout);
        snapshot.tfReady = snapshot.tfRecent || hasTransformChainReady();
        if (!snapshot.tfReady && snapshot.tfStaticReceived) {
            snapshot.tfReady = hasTransformChainReady();
        }
        snapshot.scanHealth = snapshot.scanPublisherCount == 0
            ? TopicHealth::Unavailable
            : (!snapshot.scanReceived ? TopicHealth::Waiting
                                      : (snapshot.scanRecent ? TopicHealth::Ready : TopicHealth::Stale));
        snapshot.odomHealth = snapshot.odomPublisherCount == 0
            ? TopicHealth::Unavailable
            : (!snapshot.odomReceived ? TopicHealth::Waiting
                                      : (snapshot.odomRecent ? TopicHealth::Ready : TopicHealth::Stale));
        snapshot.tfHealth = snapshot.tfPublisherCount == 0
            ? TopicHealth::Unavailable
            : (!snapshot.tfReady ? TopicHealth::Waiting : TopicHealth::Ready);
        snapshot.tfStaticHealth = snapshot.tfStaticPublisherCount == 0
            ? TopicHealth::Unavailable
            : (!snapshot.tfStaticReceived ? TopicHealth::Waiting : TopicHealth::Ready);
        snapshot.odomUsable = snapshot.odomPublisherCount > 0 && snapshot.odomReceived;
        snapshot.readyForMapping =
            snapshot.scanReceived && snapshot.scanRecent
            && snapshot.odomUsable
            && snapshot.tfReady
            && snapshot.tfStaticPublisherCount > 0;
        snapshot.odomMessageCount = m_odomMessageCount.load();
        if (snapshot.odomReceived) {
            snapshot.odomLastAgeSec = std::chrono::duration<double>(now - m_lastOdomAt).count();
        }
        snapshot.odomFrameId = m_lastOdomFrameId;
        snapshot.odomChildFrameId = m_lastOdomChildFrameId;
        return snapshot;
    }

    QString realtimeReadinessText() const
    {
        const TopicStateSnapshot snapshot = topicStateSnapshot();
        if (snapshot.readyForMapping) {
            return QStringLiteral("/scan, /odom, TF 준비 완료");
        }
        return readinessFailureMessage(snapshot);
    }

private:
    bool hasTransformChainReady() const
    {
        static constexpr const char *kBaseFrames[] = {"base_link", "base_footprint"};
        static constexpr const char *kLaserFrames[] = {"base_scan", "laser", "base_laser"};

        for (const char *baseFrame : kBaseFrames) {
            if (m_tfBuffer->canTransform("odom", baseFrame, tf2::TimePointZero)) {
                return true;
            }
            for (const char *laserFrame : kLaserFrames) {
                if (m_tfBuffer->canTransform(baseFrame, laserFrame, tf2::TimePointZero)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool tryLookupRobotPose(double &worldX, double &worldY)
    {
        static constexpr const char *kBaseFrames[] = {"base_link", "base_footprint"};

        for (const char *baseFrame : kBaseFrames) {
            try {
                const auto transform = m_tfBuffer->lookupTransform("map", baseFrame, tf2::TimePointZero);
                worldX = transform.transform.translation.x;
                worldY = transform.transform.translation.y;
                return true;
            } catch (const tf2::TransformException &) {
            }
        }

        return false;
    }

    void handleMap(const nav_msgs::msg::OccupancyGrid::ConstSharedPtr &msg)
    {
        const int width = static_cast<int>(msg->info.width);
        const int height = static_cast<int>(msg->info.height);
        const size_t expectedSize = (width > 0 && height > 0)
            ? static_cast<size_t>(width) * static_cast<size_t>(height)
            : 0;
        if (width <= 0 || height <= 0 || msg->data.size() != expectedSize) {
            if (width != m_lastInvalidWidth || height != m_lastInvalidHeight || msg->data.size() != m_lastInvalidDataSize) {
                RCLCPP_WARN(m_node->get_logger(),
                            "Invalid /map received: width=%d height=%d data_size=%zu expected=%zu",
                            width,
                            height,
                            msg->data.size(),
                            expectedSize);
                m_lastInvalidWidth = width;
                m_lastInvalidHeight = height;
                m_lastInvalidDataSize = msg->data.size();
            }
            if (onStatusUpdated) {
                onStatusUpdated(QStringLiteral("/map 토픽 형식이 올바르지 않습니다."));
            }
            return;
        }

        logMapPublishersOnce();

        if (!topicStateSnapshot().readyForMapping) {
            RCLCPP_WARN(
                m_node->get_logger(),
                "[MapSource] ignoring /map until bringup inputs are ready");
            if (onStatusUpdated) {
                onStatusUpdated(QStringLiteral("Bringup 입력이 준비되지 않아 /map 표시를 보류합니다."));
            }
            return;
        }

        if (!m_hasLoggedMapInfo
            || static_cast<uint32_t>(width) != m_lastLoggedWidth
            || static_cast<uint32_t>(height) != m_lastLoggedHeight
            || std::abs(msg->info.resolution - m_lastLoggedResolution) > 1e-9
            || std::abs(msg->info.origin.position.x - m_lastLoggedOriginX) > 1e-9
            || std::abs(msg->info.origin.position.y - m_lastLoggedOriginY) > 1e-9
            || msg->data.size() != m_lastLoggedDataSize) {
            if (m_hasLoggedMapInfo) {
                RCLCPP_INFO(m_node->get_logger(),
                            "Map geometry changed: %ux%u -> %dx%d, resolution %.4f -> %.4f, origin (%.3f, %.3f) -> (%.3f, %.3f), data_size %zu -> %zu",
                            m_lastLoggedWidth,
                            m_lastLoggedHeight,
                            width,
                            height,
                            m_lastLoggedResolution,
                            msg->info.resolution,
                            m_lastLoggedOriginX,
                            m_lastLoggedOriginY,
                            msg->info.origin.position.x,
                            msg->info.origin.position.y,
                            m_lastLoggedDataSize,
                            msg->data.size());
            } else {
                RCLCPP_INFO(m_node->get_logger(),
                            "[Map] callback received width=%d height=%d resolution=%.4f origin=(%.3f, %.3f) data_size=%zu",
                            width,
                            height,
                            msg->info.resolution,
                            msg->info.origin.position.x,
                            msg->info.origin.position.y,
                            msg->data.size());
            }
            m_hasLoggedMapInfo = true;
            m_lastLoggedWidth = static_cast<uint32_t>(width);
            m_lastLoggedHeight = static_cast<uint32_t>(height);
            m_lastLoggedResolution = msg->info.resolution;
            m_lastLoggedOriginX = msg->info.origin.position.x;
            m_lastLoggedOriginY = msg->info.origin.position.y;
            m_lastLoggedDataSize = msg->data.size();
        }

        m_mapWidth = width;
        m_mapHeight = height;
        m_mapResolution = msg->info.resolution;
        m_mapOriginX = msg->info.origin.position.x;
        m_mapOriginY = msg->info.origin.position.y;
        m_mapReceived = true;
        if (!m_hasLoggedFirstAcceptedMap) {
            RCLCPP_INFO(m_node->get_logger(), "[Cartographer] first real map received");
            m_hasLoggedFirstAcceptedMap = true;
        }

        int exploredCells = 0;
        for (const int8_t cellValue : msg->data) {
            if (static_cast<int>(cellValue) >= 0) {
                ++exploredCells;
            }
        }
        {
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_latestMap = msg;
            m_mapDirty = true;
        }

        const double exploredRatio = static_cast<double>(exploredCells) / static_cast<double>(width * height);
        if (onStatusUpdated) {
            onStatusUpdated(QStringLiteral("실시간 Cartographer 맵 수신 중 (%1 x %2, 탐색 %.1f%)")
                                .arg(width)
                                .arg(height)
                                .arg(exploredRatio * 100.0, 0, 'f', 1));
        }
    }

    void handleOdom(const nav_msgs::msg::Odometry::ConstSharedPtr &msg)
    {
        constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
        constexpr double kPreviewSpanMeters = 4.0;

        const auto &q = msg->pose.pose.orientation;
        const double sinyCosp = 2.0 * (q.w * q.z + q.x * q.y);
        const double cosyCosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
        const double yawRad = std::atan2(sinyCosp, cosyCosp);
        const qreal headingDegrees = static_cast<qreal>(90.0 - (yawRad * kRadToDeg));

        m_latestOdomX = msg->pose.pose.position.x;
        m_latestOdomY = msg->pose.pose.position.y;
        m_latestYawRad = yawRad;
        m_hasLatestOdom = true;
        ++m_odomMessageCount;
        m_lastOdomAt = std::chrono::steady_clock::now();
        m_lastOdomFrameId = QString::fromStdString(msg->header.frame_id);
        m_lastOdomChildFrameId = QString::fromStdString(msg->child_frame_id);

        if (!m_hasLoggedFirstOdom) {
            m_hasLoggedFirstOdom = true;
            RCLCPP_INFO(
                m_node->get_logger(),
                "[SLAM] first /odom received frame_id=%s child_frame_id=%s",
                msg->header.frame_id.c_str(),
                msg->child_frame_id.c_str());
        }

        double normalizedX = 0.5;
        double normalizedY = 0.5;

        if (m_mapReceived && m_mapResolution > 0.0) {
            double worldX = 0.0;
            double worldY = 0.0;
            if (tryLookupRobotPose(worldX, worldY)) {
                normalizedX = (worldX - m_mapOriginX) / (m_mapResolution * static_cast<double>(m_mapWidth));
                normalizedY = 1.0 - ((worldY - m_mapOriginY) / (m_mapResolution * static_cast<double>(m_mapHeight)));
                m_hasMapAnchor = true;
                {
                    std::lock_guard<std::mutex> lock(m_dataMutex);
                    m_latestRobotPose.valid = true;
                    m_latestRobotPose.normalizedPos = QPointF(std::clamp(normalizedX, 0.0, 1.0),
                                                              std::clamp(normalizedY, 0.0, 1.0));
                    m_latestRobotPose.headingDegrees = headingDegrees;
                }
            } else {
                m_hasMapAnchor = false;
            }
        } else {
            if (!m_hasInitialOdom) {
                m_initialOdomX = m_latestOdomX;
                m_initialOdomY = m_latestOdomY;
                m_hasInitialOdom = true;
            }

            const double deltaX = m_latestOdomX - m_initialOdomX;
            const double deltaY = m_latestOdomY - m_initialOdomY;
            normalizedX = 0.5 + (deltaX / kPreviewSpanMeters);
            normalizedY = 0.5 - (deltaY / kPreviewSpanMeters);
            std::lock_guard<std::mutex> lock(m_dataMutex);
            m_latestRobotPose.valid = true;
            m_latestRobotPose.normalizedPos = QPointF(std::clamp(normalizedX, 0.0, 1.0),
                                                      std::clamp(normalizedY, 0.0, 1.0));
            m_latestRobotPose.headingDegrees = headingDegrees;
        }

        if (!m_mapReceived && onStatusUpdated) {
            onStatusUpdated(QStringLiteral("새 Cartographer 맵 수신 대기 중"));
        } else if (m_mapReceived && !m_hasMapAnchor && onStatusUpdated) {
            onStatusUpdated(QStringLiteral("Cartographer 맵 수신 중 · 로봇 기준점을 맞추는 중입니다."));
        }
    }

    void handleScan(const sensor_msgs::msg::LaserScan::ConstSharedPtr &msg)
    {
        (void)msg;
        m_hasScan = true;
        m_lastScanAt = std::chrono::steady_clock::now();
    }

    void handleTf(const tf2_msgs::msg::TFMessage::ConstSharedPtr &msg, bool isStatic)
    {
        if (msg->transforms.empty()) {
            return;
        }
        if (isStatic) {
            m_hasTfStatic = true;
            m_lastTfStaticAt = std::chrono::steady_clock::now();
        } else {
            m_hasTf = true;
            m_lastTfAt = std::chrono::steady_clock::now();
        }
    }

    rclcpp::Node::SharedPtr m_node;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr m_cmdVelPublisher;
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr m_mapSubscription;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr m_odomSubscription;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr m_scanSubscription;
    rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr m_tfSubscription;
    rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr m_tfStaticSubscription;
    std::unique_ptr<tf2_ros::Buffer> m_tfBuffer;
    std::shared_ptr<tf2_ros::TransformListener> m_tfListener;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::thread m_spinThread;
    std::atomic<bool> m_running;
    std::mutex m_dataMutex;
    nav_msgs::msg::OccupancyGrid::ConstSharedPtr m_latestMap;
    bool m_mapDirty {false};
    RobotPoseSnapshot m_latestRobotPose;
    bool m_mapReceived;
    bool m_hasScan {false};
    bool m_hasTf {false};
    bool m_hasTfStatic {false};
    bool m_hasPublisherInfoLogged {false};
    bool m_hasLoggedFirstAcceptedMap {false};
    bool m_hasLoggedMapInfo {false};
    uint32_t m_lastLoggedWidth {0};
    uint32_t m_lastLoggedHeight {0};
    double m_lastLoggedResolution {0.0};
    double m_lastLoggedOriginX {0.0};
    double m_lastLoggedOriginY {0.0};
    size_t m_lastLoggedDataSize {0};
    int m_lastInvalidWidth {-1};
    int m_lastInvalidHeight {-1};
    size_t m_lastInvalidDataSize {0};
    int m_mapWidth {0};
    int m_mapHeight {0};
    double m_mapResolution {0.0};
    double m_mapOriginX {0.0};
    double m_mapOriginY {0.0};
    bool m_hasInitialOdom {false};
    double m_initialOdomX {0.0};
    double m_initialOdomY {0.0};
    bool m_hasLatestOdom {false};
    double m_latestOdomX {0.0};
    double m_latestOdomY {0.0};
    double m_latestYawRad {0.0};
    bool m_hasMapAnchor {false};
    bool m_hasLoggedFirstOdom {false};
    std::chrono::steady_clock::time_point m_lastScanAt {};
    std::chrono::steady_clock::time_point m_lastOdomAt {};
    std::chrono::steady_clock::time_point m_lastTfAt {};
    std::chrono::steady_clock::time_point m_lastTfStaticAt {};
    std::atomic<uint64_t> m_odomMessageCount {0};
    QString m_lastOdomFrameId;
    QString m_lastOdomChildFrameId;

public:
    bool hasReceivedRealMap() const
    {
        return m_mapReceived;
    }

private:
    void logMapPublishersOnce()
    {
        const auto publishers = m_node->get_publishers_info_by_topic("/map");
        if (!m_hasPublisherInfoLogged) {
            m_hasPublisherInfoLogged = true;
            if (publishers.empty()) {
                RCLCPP_WARN(m_node->get_logger(), "[MapSource] no /map publishers found");
            } else {
                for (const auto &info : publishers) {
                    RCLCPP_INFO(
                        m_node->get_logger(),
                        "[MapSource] /map publisher node=%s namespace=%s type=%s depth=%zu reliability=%d durability=%d",
                        info.node_name().c_str(),
                        info.node_namespace().c_str(),
                        info.topic_type().c_str(),
                        info.qos_profile().depth(),
                        static_cast<int>(info.qos_profile().reliability()),
                        static_cast<int>(info.qos_profile().durability()));
                }
            }
        }

        if (publishers.size() > 1) {
            RCLCPP_WARN(m_node->get_logger(), "[MapSource] multiple /map publishers detected: %zu", publishers.size());
        }
    }
};

SlamMappingPage::SlamMappingPage(QWidget *parent)
    : QWidget(parent)
    , m_mapView(nullptr)
    , m_statusBannerTitleLabel(nullptr)
    , m_statusBannerDetailLabel(nullptr)
    , m_mappingStatusValueLabel(nullptr)
    , m_saveFormatValueLabel(nullptr)
    , m_statusMessageLabel(nullptr)
    , m_liveStatusMessageLabel(nullptr)
    , m_logView(nullptr)
    , m_startButton(nullptr)
    , m_saveButton(nullptr)
    , m_resetButton(nullptr)
    , m_slamProcess(new QProcess(this))
    , m_saveProcess(new QProcess(this))
    , m_manualDriveTimer(new QTimer(this))
    , m_mapRefreshTimer(new QTimer(this))
    , m_mapTimeoutTimer(new QTimer(this))
    , m_mapBridge(std::make_unique<SlamMapBridge>())
    , m_mappingActive(false)
    , m_manualLinearX(0.0)
    , m_manualAngularZ(0.0)
{
    setFocusPolicy(Qt::StrongFocus);
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildStatusBanner());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);

    m_mapView = new MapViewWidget(canvas);
    m_mapView->setMinimumHeight(250);
    m_mapView->setMode(MapViewMode::Live);
    m_mapView->setOverlayChipsVisible(false);
    m_mapView->setCaption(QStringLiteral("매핑 시작 전 · Cartographer 대기 화면"));
    m_mapView->setRecording(false);
    m_mapView->setRobotPose(QPointF(0.5, 0.5), 0.0);
    m_mapView->setRobotState(RobotMarkerState::Ready);
    m_mapView->setPlannedPath({});
    leftCol->addWidget(m_mapView, 1);
    leftCol->addWidget(buildActionRow());
    bodyRow->addLayout(leftCol, 7);

    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);
    rightCol->addWidget(buildMappingStatusPanel());
    rightCol->addWidget(buildLogPanel(), 1);
    rightCol->addWidget(buildDirectionPad());
    rightCol->addWidget(buildSpeedLimitPanel());

    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(200);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    layout->addLayout(bodyRow, 1);

    connect(m_slamProcess, &QProcess::stateChanged, this, &SlamMappingPage::handleSlamProcessStateChanged);
    connect(m_slamProcess, &QProcess::errorOccurred, this, &SlamMappingPage::handleSlamProcessErrorOccurred);
    connect(m_slamProcess, &QProcess::readyReadStandardOutput, this, &SlamMappingPage::handleSlamProcessStdout);
    connect(m_slamProcess, &QProcess::readyReadStandardError, this, &SlamMappingPage::handleSlamProcessStderr);
    connect(m_slamProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &SlamMappingPage::handleSlamProcessFinished);
    connect(m_slamProcess, &QProcess::started, this, [this]() {
        qInfo() << "[SLAM] Cartographer process started";
        appendUiLog(QStringLiteral("[SLAM] Cartographer 프로세스 시작"));
        setMappingUiState(MappingUiState::WaitingForMap, QStringLiteral("/map 토픽 대기 중"));
        setLiveStatusMessage(QStringLiteral("Cartographer 프로세스 시작됨 · /map을 기다리는 중"));
        m_mapTimeoutTimer->start(kMapTimeoutMs);
        updateSlamUiState();
    });
    connect(m_saveProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &SlamMappingPage::handleSaveProcessFinished);
    connect(m_saveProcess, &QProcess::errorOccurred, this, &SlamMappingPage::handleSaveProcessErrorOccurred);
    m_manualDriveTimer->setInterval(100);
    connect(m_manualDriveTimer, &QTimer::timeout, this, &SlamMappingPage::handleManualDriveTick);
    m_mapRefreshTimer->setInterval(kMapRefreshIntervalMs);
    connect(m_mapRefreshTimer, &QTimer::timeout, this, &SlamMappingPage::handleMapRefreshTick);
    m_mapTimeoutTimer->setSingleShot(true);
    connect(m_mapTimeoutTimer, &QTimer::timeout, this, &SlamMappingPage::handleMapTimeout);

    attachMapBridgeCallbacks();
    m_mapBridge->start();
    refreshSensorStatus();
    updateSlamUiState();
    appendUiLog(QStringLiteral("[SLAM] SLAM 페이지 초기화 완료"));
}

SlamMappingPage::~SlamMappingPage()
{
    if (m_saveProcess->state() != QProcess::NotRunning) {
        m_saveProcess->kill();
        m_saveProcess->waitForFinished(1000);
    }
    m_mapTimeoutTimer->stop();
    m_mapRefreshTimer->stop();
    if (m_slamProcess->state() != QProcess::NotRunning) {
        m_slamProcess->terminate();
        if (!m_slamProcess->waitForFinished(2000)) {
            m_slamProcess->kill();
            m_slamProcess->waitForFinished(1000);
        }
    }
    m_mapBridge->stop();
}

void SlamMappingPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    setFocus(Qt::OtherFocusReason);
    qInfo() << "[TELEOP] keyboard focus acquired";
    appendUiLog(QStringLiteral("[TELEOP] keyboard focus acquired"));
    refreshSensorStatus();
}

void SlamMappingPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    stopManualDrive();
}

void SlamMappingPage::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    qInfo() << "[TELEOP] keyboard focus acquired";
    appendUiLog(QStringLiteral("[TELEOP] keyboard focus acquired"));
}

void SlamMappingPage::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    stopManualDrive();
    qWarning() << "[TELEOP] keyboard focus lost";
    appendUiLog(QStringLiteral("[TELEOP] keyboard focus lost"));
}

bool SlamMappingPage::handleManualKeyPress(int key)
{
    return consumeManualKey(key);
}

bool SlamMappingPage::handleManualKeyRelease(int key)
{
    return releaseManualKey(key);
}

QWidget *SlamMappingPage::buildStatusBanner()
{
    auto *panel = panelFrame(QStringLiteral("anomalyCard"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(8);

    m_statusBannerTitleLabel = makeLabel(QStringLiteral("🗺  Cartographer 대기 중"));
    m_statusBannerTitleLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Cyan.name()));
    layout->addWidget(m_statusBannerTitleLabel);
    m_statusBannerDetailLabel = makeLabel(QStringLiteral("TurtleBot3 bringup 후 매핑 시작 버튼으로 Cartographer를 실행합니다."),
                                          QStringLiteral("metricCaption"));
    layout->addWidget(m_statusBannerDetailLabel);
    layout->addStretch();
    layout->addWidget(makeBadgeLabel(QStringLiteral("Cartographer 활성"), QStringLiteral("cyan"), panel));
    return panel;
}

QWidget *SlamMappingPage::buildActionRow()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    m_startButton = new QPushButton(QStringLiteral("지도 매핑"));
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (m_slamProcess->state() == QProcess::NotRunning) {
            handleStartMapping();
            return;
        }
        handleResetMapping();
    });

    m_saveButton = new QPushButton(QStringLiteral("지도 저장"));
    m_saveButton->setObjectName(QStringLiteral("primaryCtaButton"));
    connect(m_saveButton, &QPushButton::clicked, this, &SlamMappingPage::handleSaveMap);

    for (QPushButton *button : {m_startButton, m_saveButton}) {
        layout->addWidget(button);
    }
    return row;
}

QWidget *SlamMappingPage::buildMappingStatusPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("매핑 상태"), QStringLiteral("metricCaption")));

    statRow(layout, QStringLiteral("지도 커버리지"), QStringLiteral("실행 중 확인"), DashboardPalette::Cyan);
    layout->addWidget(makeProportionalBar(62, DashboardPalette::Cyan, panel));
    layout->addSpacing(4);
    m_mappingStatusValueLabel = makeLabel(QStringLiteral("비활성"));
    m_mappingStatusValueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Amber.name()));
    auto *mappingStatusRow = new QHBoxLayout;
    mappingStatusRow->addWidget(makeLabel(QStringLiteral("Cartographer 상태"), QStringLiteral("metricCaption")));
    mappingStatusRow->addStretch();
    mappingStatusRow->addWidget(m_mappingStatusValueLabel);
    layout->addLayout(mappingStatusRow);

    statRow(layout, QStringLiteral("맵 저장 위치"), QStringLiteral("maps/"), DashboardPalette::Green);
    m_saveFormatValueLabel = makeLabel(QStringLiteral("png/yaml"));
    m_saveFormatValueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Green.name()));
    auto *saveFormatRow = new QHBoxLayout;
    saveFormatRow->addWidget(makeLabel(QStringLiteral("저장 형식"), QStringLiteral("metricCaption")));
    saveFormatRow->addStretch();
    saveFormatRow->addWidget(m_saveFormatValueLabel);
    layout->addLayout(saveFormatRow);

    m_statusMessageLabel = makeLabel(QStringLiteral("저장된 지도 없음"), QStringLiteral("pageSubtitle"));
    layout->addWidget(m_statusMessageLabel);
    m_liveStatusMessageLabel = makeLabel(QStringLiteral("실시간 상태 대기 중"), QStringLiteral("metricCaption"));
    layout->addWidget(m_liveStatusMessageLabel);
    return panel;
}

QWidget *SlamMappingPage::buildDirectionPad()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QGridLayout(panel);
    layout->setContentsMargins(9, 10, 9, 10);
    layout->setSpacing(5);

    auto *forwardButton = directionButton(QStringLiteral("▲"));
    auto *leftButton = directionButton(QStringLiteral("↺"));
    auto *stopButton = directionButton(QStringLiteral("정지"), true);
    auto *rightButton = directionButton(QStringLiteral("↻"));
    auto *backwardButton = directionButton(QStringLiteral("▼"));

    for (QPushButton *button : {forwardButton, backwardButton, leftButton, rightButton, stopButton}) {
        button->setFocusPolicy(Qt::NoFocus);
    }

    connect(forwardButton, &QPushButton::pressed, this, [this]() {
        m_upPressed = true;
        m_downPressed = false;
        m_leftPressed = false;
        m_rightPressed = false;
        applyManualKeyState();
    });
    connect(forwardButton, &QPushButton::released, this, [this]() {
        m_upPressed = false;
        applyManualKeyState();
    });
    connect(backwardButton, &QPushButton::pressed, this, [this]() {
        m_downPressed = true;
        m_upPressed = false;
        m_leftPressed = false;
        m_rightPressed = false;
        applyManualKeyState();
    });
    connect(backwardButton, &QPushButton::released, this, [this]() {
        m_downPressed = false;
        applyManualKeyState();
    });
    connect(leftButton, &QPushButton::pressed, this, [this]() {
        m_leftPressed = true;
        m_upPressed = false;
        m_downPressed = false;
        m_rightPressed = false;
        applyManualKeyState();
    });
    connect(leftButton, &QPushButton::released, this, [this]() {
        m_leftPressed = false;
        applyManualKeyState();
    });
    connect(rightButton, &QPushButton::pressed, this, [this]() {
        m_rightPressed = true;
        m_upPressed = false;
        m_downPressed = false;
        m_leftPressed = false;
        applyManualKeyState();
    });
    connect(rightButton, &QPushButton::released, this, [this]() {
        m_rightPressed = false;
        applyManualKeyState();
    });
    connect(stopButton, &QPushButton::clicked, this, &SlamMappingPage::stopManualDrive);

    layout->addWidget(forwardButton, 0, 1);
    layout->addWidget(leftButton, 1, 0);
    layout->addWidget(stopButton, 1, 1);
    layout->addWidget(rightButton, 1, 2);
    layout->addWidget(backwardButton, 2, 1);
    return panel;
}

QWidget *SlamMappingPage::buildLogPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("실행 로그"), QStringLiteral("metricCaption")));

    m_logView = new QPlainTextEdit(panel);
    m_logView->setReadOnly(true);
    m_logView->setObjectName(QStringLiteral("slamLogView"));
    m_logView->setMinimumHeight(140);
    m_logView->setMaximumBlockCount(200);
    layout->addWidget(m_logView);
    return panel;
}

QWidget *SlamMappingPage::buildSpeedLimitPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(makeLabel(QStringLiteral("속도 제한"), QStringLiteral("metricCaption")));
    topRow->addStretch();
    auto *valueLabel = makeLabel(QStringLiteral("%1 m/s · %2 rad/s")
                                     .arg(kManualLinearSpeedMps, 0, 'f', 2)
                                     .arg(kManualAngularSpeedRadPerSec, 0, 'f', 2));
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Cyan.name()));
    topRow->addWidget(valueLabel);
    layout->addLayout(topRow);
    layout->addWidget(makeProportionalBar(50, DashboardPalette::Cyan, panel));
    layout->addStretch();
    return panel;
}

void SlamMappingPage::attachMapBridgeCallbacks()
{
    m_mapBridge->onStatusUpdated = [this](const QString &message) {
        QMetaObject::invokeMethod(this, [this, message]() { handleBridgeStatusUpdated(message); }, Qt::QueuedConnection);
    };
}

void SlamMappingPage::resetMappingView(const QString &caption, bool mappingRunning)
{
    m_hasReceivedRealMap = false;
    m_mapView->clearBackgroundImage();
    m_mapView->setPlannedPath({});
    m_mapView->setCaption(caption);
    m_mapView->setRobotVisible(false);
    m_mapView->setRobotPose(QPointF(0.5, 0.5), 0.0);
    m_mapView->setRobotState(mappingRunning ? RobotMarkerState::Moving : RobotMarkerState::Ready);
    setLiveStatusMessage(mappingRunning ? QStringLiteral("새 Cartographer 맵 수신 대기 중")
                                        : QStringLiteral("실시간 상태 대기 중"));
}

void SlamMappingPage::setMappingUiState(MappingUiState state, const QString &detail)
{
    m_mappingUiState = state;
    m_mappingUiDetail = detail;
    if (!detail.isEmpty()) {
        appendUiLog(QStringLiteral("[SLAM] %1").arg(detail));
    }
    updateSlamUiState();
}

void SlamMappingPage::refreshSensorStatus()
{
    if (m_mapBridge == nullptr) {
        return;
    }

    const TopicStateSnapshot snapshot = m_mapBridge->topicStateSnapshot();
    qInfo() << "[SLAM] scan ready:" << snapshot.scanReceived << snapshot.scanRecent;
    qInfo() << "[SLAM] odom ready:" << snapshot.odomReceived << snapshot.odomRecent;
    qInfo() << "[SLAM] tf ready:" << snapshot.tfReady
            << "tf=" << snapshot.tfReceived << snapshot.tfRecent
            << "tf_static=" << snapshot.tfStaticReceived;
    appendUiLog(QStringLiteral("[SLAM] Bringup 상태 확인"));
    appendUiLog(QStringLiteral("[SLAM] /scan publisher 수: %1").arg(snapshot.scanPublisherCount));
    appendUiLog(QStringLiteral("[SLAM] /scan 수신 확인: %1").arg(topicHealthText(snapshot.scanHealth)));
    appendUiLog(QStringLiteral("[SLAM] /odom publisher 수: %1").arg(snapshot.odomPublisherCount));
    appendUiLog(QStringLiteral("[SLAM] /odom 최초 수신 여부: %1").arg(snapshot.odomReceived ? QStringLiteral("true") : QStringLiteral("false")));
    appendUiLog(QStringLiteral("[SLAM] /odom 메시지 수신 횟수: %1").arg(snapshot.odomMessageCount));
    if (snapshot.odomReceived && snapshot.odomLastAgeSec >= 0.0) {
        appendUiLog(QStringLiteral("[SLAM] /odom 마지막 수신 경과 시간: %1초").arg(snapshot.odomLastAgeSec, 0, 'f', 1));
    }
    appendUiLog(QStringLiteral("[SLAM] /odom 수신 확인: %1").arg(topicHealthText(snapshot.odomHealth)));
    appendUiLog(QStringLiteral("[SLAM] /tf publisher 수: %1").arg(snapshot.tfPublisherCount));
    appendUiLog(QStringLiteral("[SLAM] /tf 수신 확인: %1").arg(topicHealthText(snapshot.tfHealth)));
    appendUiLog(QStringLiteral("[SLAM] /tf_static publisher 수: %1").arg(snapshot.tfStaticPublisherCount));
    appendUiLog(QStringLiteral("[SLAM] /tf_static 수신 확인: %1").arg(topicHealthText(snapshot.tfStaticHealth)));

    if (snapshot.odomHealth == TopicHealth::Stale && snapshot.odomUsable) {
        appendUiLog(QStringLiteral("[SLAM][WARN] /odom의 최근 갱신이 없습니다."));
        appendUiLog(QStringLiteral("[SLAM][WARN] publisher와 이전 수신 데이터가 확인되어 매핑을 계속 시작합니다."));
    }

    if (snapshot.readyForMapping) {
        if (m_mappingUiState == MappingUiState::Idle || m_mappingUiState == MappingUiState::CheckingBringup) {
            setMappingUiState(MappingUiState::SensorsReady, QStringLiteral("센서 준비 완료"));
        }
        setLiveStatusMessage(QStringLiteral("/scan, /odom, TF 준비 완료"));
        return;
    }

    if (m_slamProcess->state() == QProcess::NotRunning && !m_mappingActive) {
        setMappingUiState(MappingUiState::CheckingBringup, QStringLiteral("Bringup 연결 확인 중"));
    }
    setLiveStatusMessage(readinessFailureMessage(snapshot));
}

void SlamMappingPage::appendUiLog(const QString &message)
{
    if (m_logView == nullptr || message.isEmpty()) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString line = QStringLiteral("[%1] %2").arg(timestamp, message);
    const QString current = m_logView->toPlainText();
    const QString lastLine = current.section(QLatin1Char('\n'), -1);
    if (lastLine == line) {
        return;
    }
    m_logView->appendPlainText(line);
}

void SlamMappingPage::updateSlamUiState()
{
    const bool slamRunning = (m_slamProcess->state() == QProcess::Running);
    const bool slamStarting = (m_slamProcess->state() == QProcess::Starting);
    const bool saveRunning = (m_saveProcess->state() != QProcess::NotRunning);

    QString bannerTitle = QStringLiteral("🗺  Cartographer 대기 중");
    QString bannerDetail = QStringLiteral("TurtleBot3 bringup 후 매핑 시작 버튼으로 Cartographer를 실행합니다.");
    QString mappingValue = QStringLiteral("비활성");
    QColor mappingColor = DashboardPalette::Amber;

    switch (m_mappingUiState) {
    case MappingUiState::Idle:
        break;
    case MappingUiState::CheckingBringup:
        bannerTitle = QStringLiteral("🗺  Bringup 연결 확인 중");
        bannerDetail = QStringLiteral("매핑 시작 전 /scan, /odom, /tf, /tf_static 수신 상태를 확인합니다.");
        mappingValue = QStringLiteral("준비 확인 중");
        mappingColor = DashboardPalette::Amber;
        break;
    case MappingUiState::SensorsReady:
        bannerTitle = QStringLiteral("🗺  센서 준비 완료");
        bannerDetail = QStringLiteral("Bringup 입력이 준비되었습니다. 매핑 시작을 누르면 Cartographer를 실행합니다.");
        mappingValue = QStringLiteral("센서 준비 완료");
        mappingColor = DashboardPalette::Green;
        break;
    case MappingUiState::Starting:
        bannerTitle = QStringLiteral("🗺  Cartographer 시작 중");
        bannerDetail = QStringLiteral("ROS 환경을 로드하고 Cartographer launch를 실행하는 중입니다.");
        mappingValue = QStringLiteral("시작 중");
        mappingColor = DashboardPalette::Cyan;
        break;
    case MappingUiState::WaitingForMap:
        bannerTitle = QStringLiteral("🗺  /map 토픽 대기 중");
        bannerDetail = QStringLiteral("Cartographer 프로세스는 시작되었지만 아직 실제 OccupancyGrid를 받지 못했습니다.");
        mappingValue = QStringLiteral("/map 대기");
        mappingColor = DashboardPalette::Cyan;
        break;
    case MappingUiState::Running:
        bannerTitle = QStringLiteral("🗺  Cartographer 매핑 실행 중");
        bannerDetail = QStringLiteral("실제 /map을 수신 중이며 로봇 이동에 따라 지도가 확장됩니다.");
        mappingValue = QStringLiteral("실행 중");
        mappingColor = DashboardPalette::Green;
        break;
    case MappingUiState::Error:
        bannerTitle = QStringLiteral("🗺  Cartographer 오류");
        bannerDetail = QStringLiteral("프로세스 로그 또는 센서 연결 상태를 확인하세요.");
        mappingValue = QStringLiteral("오류");
        mappingColor = DashboardPalette::Red;
        break;
    }

    if (m_statusBannerTitleLabel != nullptr) {
        m_statusBannerTitleLabel->setText(bannerTitle);
    }
    if (m_statusBannerDetailLabel != nullptr) {
        if (saveRunning) {
            m_statusBannerDetailLabel->setText(QStringLiteral("현재 점유격자를 maps 폴더에 저장하는 중입니다."));
        } else {
            m_statusBannerDetailLabel->setText(bannerDetail);
        }
    }
    if (m_mappingStatusValueLabel != nullptr) {
        m_mappingStatusValueLabel->setText(mappingValue);
        m_mappingStatusValueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(mappingColor.name()));
    }
    if (m_startButton != nullptr) {
        m_startButton->setText((slamRunning || slamStarting) ? QStringLiteral("초기화")
                                                             : QStringLiteral("지도 매핑"));
        m_startButton->setEnabled(!saveRunning);
    }
    if (m_saveButton != nullptr) {
        m_saveButton->setEnabled((slamRunning || slamStarting) && !saveRunning);
    }
    if (m_resetButton != nullptr) {
        m_resetButton->setEnabled(!saveRunning);
    }
    if (m_saveFormatValueLabel != nullptr && !m_lastSavedBasePath.isEmpty()) {
        m_saveFormatValueLabel->setText(QStringLiteral("png/yaml · %1").arg(QFileInfo(m_lastSavedBasePath).baseName()));
    }
    if (!m_mappingUiDetail.isEmpty() && !saveRunning) {
        setStatusMessage(m_mappingUiDetail);
    }
}

void SlamMappingPage::setStatusMessage(const QString &message, const QString &kind)
{
    if (m_statusMessageLabel == nullptr) {
        return;
    }
    m_statusMessageLabel->setObjectName(kind);
    m_statusMessageLabel->setText(message);
    m_statusMessageLabel->style()->unpolish(m_statusMessageLabel);
    m_statusMessageLabel->style()->polish(m_statusMessageLabel);
}

void SlamMappingPage::setLiveStatusMessage(const QString &message, const QString &kind)
{
    if (m_liveStatusMessageLabel == nullptr) {
        return;
    }
    if (m_liveStatusMessageLabel->text() == message && m_liveStatusMessageLabel->objectName() == kind) {
        return;
    }
    m_liveStatusMessageLabel->setObjectName(kind);
    m_liveStatusMessageLabel->setText(message);
}

QString SlamMappingPage::mapsDirectoryPath() const
{
    const QString workspaceRoot = findWorkspaceRoot();
    if (workspaceRoot.isEmpty()) {
        return QString();
    }

    QDir mapsDir(workspaceRoot);
    if (!mapsDir.exists(QStringLiteral("maps")) && !mapsDir.mkdir(QStringLiteral("maps"))) {
        return QString();
    }
    return mapsDir.filePath(QStringLiteral("maps"));
}

QString SlamMappingPage::nextMapBasePath() const
{
    const QString mapsDir = mapsDirectoryPath();
    if (mapsDir.isEmpty()) {
        return QString();
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QDir(mapsDir).filePath(QStringLiteral("slam_map_%1").arg(timestamp));
}

bool SlamMappingPage::finalizeSavedMap(const QString &basePath, QString *errorMessage) const
{
    const QString pgmPath = basePath + QStringLiteral(".pgm");
    const QString pngPath = basePath + QStringLiteral(".png");
    const QString yamlPath = basePath + QStringLiteral(".yaml");

    const QImage mapImage = loadPgmImage(pgmPath);
    if (mapImage.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("저장된 PGM 파일을 읽지 못했습니다.");
        }
        return false;
    }
    if (!mapImage.save(pngPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("PNG 파일로 변환하지 못했습니다.");
        }
        return false;
    }

    const QString yamlRewriteError = rewriteYamlImagePath(yamlPath, QFileInfo(pngPath).fileName());
    if (!yamlRewriteError.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = yamlRewriteError;
        }
        return false;
    }

    QFile::remove(pgmPath);
    return true;
}

bool SlamMappingPage::startMappingProcess()
{
    qInfo() << "[SLAM] mapping start requested";
    appendUiLog(QStringLiteral("[SLAM] 매핑 시작 요청"));
    stopMappingProcess();
    m_mapBridge->beginFreshSession();
    refreshSensorStatus();

    const TopicStateSnapshot snapshot = m_mapBridge->topicStateSnapshot();
    if (!snapshot.readyForMapping) {
        const QString message = readinessFailureMessage(snapshot);
        qWarning() << "[SLAM]" << message;
        appendUiLog(QStringLiteral("[SLAM][ERROR] %1").arg(message));
        setMappingUiState(MappingUiState::Error, message);
        resetMappingView(QStringLiteral("Bringup 입력이 준비되지 않아 매핑을 시작하지 않았습니다."), false);
        setLiveStatusMessage(message);
        return false;
    }

    if (snapshot.odomHealth == TopicHealth::Stale && snapshot.odomUsable) {
        appendUiLog(QStringLiteral("[SLAM][WARN] /odom publisher는 존재하지만 최근 메시지가 없습니다."));
        appendUiLog(QStringLiteral("[SLAM][WARN] 마지막으로 수신한 odometry 데이터를 사용하여 매핑을 시작합니다."));
    }

    stopManualDrive();
    setMappingUiState(MappingUiState::Starting, QStringLiteral("Cartographer 시작 중"));
    resetMappingView(QStringLiteral("새 SLAM 세션 준비 중 · 이전 맵을 버리고 실시간 맵만 기다리는 중"), false);
    const QString turtlebotModel = QString::fromLocal8Bit(qgetenv("TURTLEBOT3_MODEL")).isEmpty()
        ? QStringLiteral("burger")
        : QString::fromLocal8Bit(qgetenv("TURTLEBOT3_MODEL"));
    const QString slamCommand = shellCommandPrefix()
        + QStringLiteral("export TURTLEBOT3_MODEL=\"%1\" && exec ros2 launch turtlebot3_cartographer cartographer.launch.py use_sim_time:=false use_rviz:=false")
              .arg(turtlebotModel);
    qInfo() << "[SLAM] starting Cartographer process:" << slamCommand;
    appendUiLog(QStringLiteral("[SLAM] Cartographer 실행 시작"));
    appendUiLog(QStringLiteral("[SLAM] Cartographer 실행 명령 준비 완료"));
    m_slamProcess->setProgram(QStringLiteral("bash"));
    m_slamProcess->setArguments({
        QStringLiteral("-lc"),
        slamCommand
    });
    m_slamProcess->start();
    if (!m_slamProcess->waitForStarted(5000)) {
        qWarning() << "[SLAM] Cartographer process failed:" << m_slamProcess->errorString();
        appendUiLog(QStringLiteral("[SLAM][ERROR] Cartographer 시작 실패: %1").arg(m_slamProcess->errorString()));
        setMappingUiState(MappingUiState::Error,
                          QStringLiteral("Cartographer 시작 실패: %1").arg(m_slamProcess->errorString()));
        return false;
    }
    m_mappingActive = true;
    m_mapRefreshTimer->start();
    resetMappingView(QStringLiteral("실시간 Cartographer 맵 수신 대기 중"), true);
    setMappingUiState(MappingUiState::WaitingForMap, QStringLiteral("Cartographer 프로세스 시작됨"));
    setLiveStatusMessage(QStringLiteral("Cartographer 실행 후 /map을 기다리는 중"));
    return true;
}

void SlamMappingPage::stopMappingProcess()
{
    stopManualDrive();
    m_mapTimeoutTimer->stop();
    m_mapRefreshTimer->stop();
    m_mappingActive = false;
    if (m_slamProcess->state() == QProcess::NotRunning) {
        return;
    }

    m_slamProcess->terminate();
    if (!m_slamProcess->waitForFinished(4000)) {
        m_slamProcess->kill();
        m_slamProcess->waitForFinished(2000);
    }
}

void SlamMappingPage::handleStartMapping()
{
    if (m_slamProcess->state() != QProcess::NotRunning) {
        setStatusMessage(QStringLiteral("Cartographer가 이미 실행 중입니다."));
        return;
    }

    setMappingUiState(MappingUiState::CheckingBringup, QStringLiteral("Bringup 연결 확인 중"));
    if (!startMappingProcess()) {
        return;
    }
    setStatusMessage(QStringLiteral("새 Cartographer 세션을 시작했습니다. 기존 맵은 가져오지 않고 새 매핑만 표시합니다."));
}

void SlamMappingPage::handleResetMapping()
{
    stopMappingProcess();
    m_lastSavedBasePath.clear();
    setMappingUiState(MappingUiState::CheckingBringup, QStringLiteral("Bringup 연결 확인 중"));
    resetMappingView(QStringLiteral("초기화 완료 · 새 Cartographer 세션을 다시 준비하는 중"), false);
    updateSlamUiState();

    if (!startMappingProcess()) {
        setStatusMessage(QStringLiteral("매핑 초기화 후 재시작에 실패했습니다."));
        return;
    }

    setStatusMessage(QStringLiteral("기존 매핑을 비우고 새 SLAM 세션으로 다시 시작했습니다."));
}

void SlamMappingPage::handleSaveMap()
{
    if (m_saveProcess->state() != QProcess::NotRunning) {
        setStatusMessage(QStringLiteral("이미 지도 저장 작업이 실행 중입니다."));
        return;
    }

    const QString basePath = nextMapBasePath();
    if (basePath.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("저장 경로 오류"),
                             QStringLiteral("workspace의 maps 폴더를 찾지 못했습니다."));
        return;
    }

    m_lastSavedBasePath = basePath;
    const QString saveCommand = shellCommandPrefix()
        + QStringLiteral("ros2 run nav2_map_server map_saver_cli -f \"%1\"").arg(basePath);
    m_saveProcess->setProgram(QStringLiteral("bash"));
    m_saveProcess->setArguments({
        QStringLiteral("-lc"),
        saveCommand
    });
    m_saveProcess->start();
    updateSlamUiState();
    setStatusMessage(QStringLiteral("지도 저장을 시작했습니다: %1").arg(QFileInfo(basePath).fileName()));
}

void SlamMappingPage::handleSlamProcessStateChanged()
{
    if (m_slamProcess->state() == QProcess::Starting) {
        appendUiLog(QStringLiteral("[SLAM] Cartographer 프로세스 시작 중"));
        setMappingUiState(MappingUiState::Starting, QStringLiteral("Cartographer 시작 중"));
    }
    updateSlamUiState();
}

void SlamMappingPage::handleSlamProcessErrorOccurred()
{
    qWarning() << "[SLAM] process failed:" << m_slamProcess->errorString();
    appendUiLog(QStringLiteral("[SLAM][ERROR] Cartographer 프로세스 오류: %1").arg(m_slamProcess->errorString()));
    setMappingUiState(MappingUiState::Error,
                      QStringLiteral("Cartographer 실행 오류: %1").arg(m_slamProcess->errorString()));
    if (!m_hasReceivedRealMap) {
        resetMappingView(QStringLiteral("Cartographer 실행 실패 · 실시간 지도를 받지 못했습니다."), false);
    }
}

void SlamMappingPage::handleSlamProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qInfo() << "[SLAM] Cartographer process finished:" << exitCode << "status=" << static_cast<int>(exitStatus);
    appendUiLog(QStringLiteral("[SLAM] Cartographer 프로세스 종료: exit=%1 status=%2")
                    .arg(exitCode)
                    .arg(static_cast<int>(exitStatus)));
    m_mapTimeoutTimer->stop();
    if (!m_hasReceivedRealMap) {
        setMappingUiState(
            MappingUiState::Error,
            QStringLiteral("Cartographer는 실행되었지만 /map을 받지 못했습니다. /scan, /odom, TF 및 occupancy_grid_node를 확인하세요."));
    } else {
        setMappingUiState(MappingUiState::Idle, QStringLiteral("Cartographer 세션이 종료되었습니다."));
    }
    if (!m_hasReceivedRealMap) {
        resetMappingView(QStringLiteral("Cartographer 종료 · 실시간 지도를 받지 못했습니다."), false);
    }
}

void SlamMappingPage::handleSlamProcessStdout()
{
    const QString output = QString::fromLocal8Bit(m_slamProcess->readAllStandardOutput());
    if (output.trimmed().isEmpty()) {
        return;
    }
    for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        qInfo().noquote() << "[SLAM][stdout]" << trimmed;
        appendUiLog(QStringLiteral("[SLAM][stdout] %1").arg(trimmed));
    }
}

void SlamMappingPage::handleSlamProcessStderr()
{
    const QString output = QString::fromLocal8Bit(m_slamProcess->readAllStandardError());
    if (output.trimmed().isEmpty()) {
        return;
    }
    for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        qWarning().noquote() << "[SLAM][stderr]" << trimmed;
        appendUiLog(QStringLiteral("[SLAM][stderr] %1").arg(trimmed));
        if (trimmed.contains(QStringLiteral("package"), Qt::CaseInsensitive)
            || trimmed.contains(QStringLiteral("launch file"), Qt::CaseInsensitive)
            || trimmed.contains(QStringLiteral("configuration"), Qt::CaseInsensitive)
            || trimmed.contains(QStringLiteral("TF"), Qt::CaseInsensitive)
            || trimmed.contains(QStringLiteral("/scan"), Qt::CaseInsensitive)
            || trimmed.contains(QStringLiteral("/odom"), Qt::CaseInsensitive)) {
            setMappingUiState(MappingUiState::Error, trimmed);
        }
    }
}

void SlamMappingPage::handleSaveProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    updateSlamUiState();
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        setStatusMessage(QStringLiteral("지도 저장 실패: %1").arg(QString::fromLocal8Bit(m_saveProcess->readAllStandardError())));
        return;
    }

    QString errorMessage;
    if (!finalizeSavedMap(m_lastSavedBasePath, &errorMessage)) {
        setStatusMessage(QStringLiteral("지도 후처리 실패: %1").arg(errorMessage));
        return;
    }

    const QString baseName = QFileInfo(m_lastSavedBasePath).baseName();
    setStatusMessage(QStringLiteral("지도 저장 완료: maps/%1.png, maps/%1.yaml").arg(baseName));
    if (m_saveFormatValueLabel != nullptr) {
        m_saveFormatValueLabel->setText(QStringLiteral("png/yaml · %1").arg(baseName));
    }
}

void SlamMappingPage::handleSaveProcessErrorOccurred()
{
    updateSlamUiState();
    setStatusMessage(QStringLiteral("지도 저장 실행 오류: %1").arg(m_saveProcess->errorString()));
}

void SlamMappingPage::handleManualDriveTick()
{
    if (m_mapBridge == nullptr) {
        qWarning() << "[TELEOP] cmd_vel publisher 없음";
        return;
    }
    m_mapBridge->publishVelocity(m_manualLinearX, m_manualAngularZ);
}

void SlamMappingPage::handleMapRefreshTick()
{
    if (!m_mappingActive || m_mapBridge == nullptr) {
        return;
    }

    if (!m_mapBridge->hasReceivedRealMap()) {
        refreshSensorStatus();
        setLiveStatusMessage(QStringLiteral("/map 토픽을 기다리는 중 · %1").arg(m_mapBridge->realtimeReadinessText()));
        return;
    }

    if (const auto mapMsg = m_mapBridge->peekLatestMap()) {
        QImage image;
        if (!createOccupancyGridImage(*mapMsg, &image)) {
            appendUiLog(QStringLiteral("[SLAM][ERROR] OccupancyGrid 이미지를 생성하지 못했습니다."));
            setStatusMessage(QStringLiteral("OccupancyGrid 이미지를 생성하지 못했습니다."));
        } else {
            if (!m_hasReceivedRealMap) {
                qInfo() << "[SLAM] first /map received";
                qInfo() << "[SLAM] map width" << image.width() << "height" << image.height();
                appendUiLog(QStringLiteral("[SLAM] 첫 번째 지도 수신"));
                appendUiLog(QStringLiteral("[SLAM] 지도 크기: %1 x %2").arg(image.width()).arg(image.height()));
                m_mapTimeoutTimer->stop();
            }
            m_hasReceivedRealMap = true;
            setMappingUiState(MappingUiState::Running, QStringLiteral("실시간 지도를 생성하고 있습니다."));
            handleMapImageUpdated(image);
            m_mapBridge->markLatestMapDisplayed();
        }
    }

    const RobotPoseSnapshot pose = m_mapBridge->takeLatestRobotPose();
    if (m_hasReceivedRealMap && pose.valid) {
        handleRobotPoseUpdated(pose.normalizedPos, pose.headingDegrees);
    }
}

void SlamMappingPage::handleMapImageUpdated(const QImage &image)
{
    if (!m_mappingActive) {
        return;
    }
    m_mapView->setBackgroundImage(image);
    m_mapView->setRobotVisible(true);
    m_mapView->setCaption(QStringLiteral("실시간 Cartographer 매핑 화면"));
    setLiveStatusMessage(QStringLiteral("실시간 지도를 생성하고 있습니다."));
    appendUiLog(QStringLiteral("[SLAM] 지도 화면 갱신"));
}

void SlamMappingPage::handleRobotPoseUpdated(const QPointF &normalizedPos, qreal headingDegrees)
{
    if (!m_mappingActive) {
        return;
    }
    if (!m_hasReceivedRealMap) {
        return;
    }
    m_mapView->setRobotPose(normalizedPos, headingDegrees);
    m_mapView->setRobotVisible(true);
    m_mapView->setRobotState(RobotMarkerState::Moving);
    appendUiLog(QStringLiteral("[SLAM] map -> base 프레임 조회 성공"));
}

void SlamMappingPage::handleBridgeStatusUpdated(const QString &message)
{
    if (!m_mappingActive) {
        return;
    }
    if (m_slamProcess->state() == QProcess::Running || m_slamProcess->state() == QProcess::Starting) {
        setLiveStatusMessage(message);
    }
}

void SlamMappingPage::handleMapTimeout()
{
    if (m_hasReceivedRealMap || m_slamProcess->state() != QProcess::Running) {
        return;
    }
    qWarning() << "[SLAM] /map timeout";
    appendUiLog(QStringLiteral("[SLAM][ERROR] /map timeout"));
    setMappingUiState(
        MappingUiState::Error,
        QStringLiteral("Cartographer는 실행되었지만 /map을 받지 못했습니다. /scan, /odom, TF 및 occupancy_grid_node를 확인하세요."));
    setLiveStatusMessage(QStringLiteral("/map timeout · /scan, /odom, TF 및 occupancy_grid_node 확인 필요"));
}

void SlamMappingPage::setManualDriveCommand(double linearX, double angularZ, const QString &statusMessage)
{
    if (m_mapBridge == nullptr) {
        qWarning() << "[TELEOP] cmd_vel publisher 없음";
        appendUiLog(QStringLiteral("[TELEOP][ERROR] cmd_vel publisher 없음"));
        return;
    }
    m_manualLinearX = linearX;
    m_manualAngularZ = angularZ;
    qInfo() << "[TELEOP] publishing cmd_vel:" << linearX << angularZ;
    m_mapBridge->publishVelocity(linearX, angularZ);
    if (!qFuzzyIsNull(m_manualLinearX) || !qFuzzyIsNull(m_manualAngularZ)) {
        m_manualDriveTimer->start();
    } else {
        m_manualDriveTimer->stop();
    }

    setStatusMessage(statusMessage);
}

void SlamMappingPage::applyManualKeyState()
{
    double linearX = 0.0;
    double angularZ = 0.0;

    if (m_upPressed == m_downPressed) {
        linearX = 0.0;
    } else if (m_upPressed) {
        linearX = kManualLinearSpeedMps;
    } else {
        linearX = -kManualLinearSpeedMps;
    }

    if (m_leftPressed == m_rightPressed) {
        angularZ = 0.0;
    } else if (m_leftPressed) {
        angularZ = kManualAngularSpeedRadPerSec;
    } else {
        angularZ = -kManualAngularSpeedRadPerSec;
    }

    if (qFuzzyIsNull(linearX) && qFuzzyIsNull(angularZ)) {
        stopManualDrive();
        return;
    }

    setManualDriveCommand(
        linearX,
        angularZ,
        QStringLiteral("수동 제어 · linear %1 m/s / angular %2 rad/s")
            .arg(linearX, 0, 'f', 2)
            .arg(angularZ, 0, 'f', 2));
}

void SlamMappingPage::stopManualDrive()
{
    m_manualDriveTimer->stop();
    m_manualLinearX = 0.0;
    m_manualAngularZ = 0.0;
    m_upPressed = false;
    m_downPressed = false;
    m_leftPressed = false;
    m_rightPressed = false;
    if (m_mapBridge != nullptr) {
        m_mapBridge->publishVelocity(0.0, 0.0);
    }
    qInfo() << "[TELEOP] stop command published";
    appendUiLog(QStringLiteral("[TELEOP] stop command published"));
}

bool SlamMappingPage::consumeManualKey(int key)
{
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_W:
        qInfo() << "[TELEOP] key pressed: W/Up";
        m_upPressed = true;
        applyManualKeyState();
        return true;
    case Qt::Key_Down:
    case Qt::Key_S:
        qInfo() << "[TELEOP] key pressed: S/Down";
        m_downPressed = true;
        applyManualKeyState();
        return true;
    case Qt::Key_Left:
    case Qt::Key_A:
        qInfo() << "[TELEOP] key pressed: A/Left";
        m_leftPressed = true;
        applyManualKeyState();
        return true;
    case Qt::Key_Right:
    case Qt::Key_D:
        qInfo() << "[TELEOP] key pressed: D/Right";
        m_rightPressed = true;
        applyManualKeyState();
        return true;
    case Qt::Key_Space:
        qInfo() << "[TELEOP] key pressed: Space";
        m_upPressed = false;
        m_downPressed = false;
        m_leftPressed = false;
        m_rightPressed = false;
        stopManualDrive();
        return true;
    default:
        return false;
    }
}

bool SlamMappingPage::releaseManualKey(int key)
{
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_W:
        m_upPressed = false;
        applyManualKeyState();
        return true;
    case Qt::Key_Down:
    case Qt::Key_S:
        m_downPressed = false;
        applyManualKeyState();
        return true;
    case Qt::Key_Left:
    case Qt::Key_A:
        m_leftPressed = false;
        applyManualKeyState();
        return true;
    case Qt::Key_Right:
    case Qt::Key_D:
        m_rightPressed = false;
        applyManualKeyState();
        return true;
    default:
        return false;
    }
}
