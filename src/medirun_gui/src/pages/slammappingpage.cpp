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
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>

#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
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

QString processStateText(QProcess::ProcessState state)
{
    switch (state) {
    case QProcess::NotRunning:
        return QStringLiteral("비활성");
    case QProcess::Starting:
        return QStringLiteral("시작 중");
    case QProcess::Running:
        return QStringLiteral("실행 중");
    }
    return QStringLiteral("알 수 없음");
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

        const auto mapQos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();

        m_mapSubscription = m_node->create_subscription<nav_msgs::msg::OccupancyGrid>(
            "/map",
            mapQos,
            [this](const nav_msgs::msg::OccupancyGrid::ConstSharedPtr msg) {
                handleMap(msg);
            });
        m_odomSubscription = m_node->create_subscription<nav_msgs::msg::Odometry>(
            "/odom",
            rclcpp::SystemDefaultsQoS(),
            [this](const nav_msgs::msg::Odometry::ConstSharedPtr msg) {
                handleOdom(msg);
            });
        m_scanSubscription = m_node->create_subscription<sensor_msgs::msg::LaserScan>(
            "/scan",
            rclcpp::SensorDataQoS(),
            [this](const sensor_msgs::msg::LaserScan::ConstSharedPtr msg) {
                handleScan(msg);
            });
        resetTracking();
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
            onStatusUpdated(QStringLiteral("Cartographer 토픽 구독 대기 중 (/map, /odom)"));
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

    void resetTracking()
    {
        m_mapReceived = false;
        m_hasInitialOdom = false;
        m_hasLatestOdom = false;
        m_hasMapAnchor = false;
        m_ignoreMapUntil = std::chrono::steady_clock::time_point();
        m_mapWidth = 0;
        m_mapHeight = 0;
        m_mapResolution = 0.0;
        m_mapOriginX = 0.0;
        m_mapOriginY = 0.0;
    }

    void beginFreshSession()
    {
        resetTracking();
        m_ignoreMapUntil = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    }

    std::function<void(const QImage &)> onMapImageUpdated;
    std::function<void(const QPointF &, qreal)> onRobotPoseUpdated;
    std::function<void(const QString &)> onStatusUpdated;

    void publishVelocity(double linearX, double angularZ)
    {
        geometry_msgs::msg::Twist cmd;
        cmd.linear.x = linearX;
        cmd.angular.z = angularZ;
        m_cmdVelPublisher->publish(cmd);
    }

private:
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
        if (m_ignoreMapUntil != std::chrono::steady_clock::time_point()
            && std::chrono::steady_clock::now() < m_ignoreMapUntil) {
            return;
        }

        const int width = static_cast<int>(msg->info.width);
        const int height = static_cast<int>(msg->info.height);
        if (width <= 0 || height <= 0 || static_cast<int>(msg->data.size()) != width * height) {
            if (onStatusUpdated) {
                onStatusUpdated(QStringLiteral("/map 토픽 형식이 올바르지 않습니다."));
            }
            return;
        }

        int exploredCells = 0;
        QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < height; ++y) {
            QRgb *scanLine = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < width; ++x) {
                const int sourceIndex = (height - 1 - y) * width + x;
                const int8_t value = msg->data.at(sourceIndex);
                if (value < 0) {
                    scanLine[x] = qRgba(44, 52, 64, 255);
                } else if (value >= 65) {
                    ++exploredCells;
                    scanLine[x] = qRgba(24, 30, 38, 255);
                } else {
                    ++exploredCells;
                    scanLine[x] = qRgba(238, 245, 255, 255);
                }
            }
        }

        m_mapWidth = width;
        m_mapHeight = height;
        m_mapResolution = msg->info.resolution;
        m_mapOriginX = msg->info.origin.position.x;
        m_mapOriginY = msg->info.origin.position.y;
        m_mapReceived = true;
        const double exploredRatio = static_cast<double>(exploredCells) / static_cast<double>(width * height);

        if (onMapImageUpdated) {
            onMapImageUpdated(image);
        }
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

        double normalizedX = 0.5;
        double normalizedY = 0.5;

        if (m_mapReceived && !m_hasMapAnchor) {
            double anchorWorldX = 0.0;
            double anchorWorldY = 0.0;
            if (tryLookupRobotPose(anchorWorldX, anchorWorldY)) {
                m_mapAnchorWorldX = anchorWorldX;
                m_mapAnchorWorldY = anchorWorldY;
                m_mapAnchorOdomX = m_latestOdomX;
                m_mapAnchorOdomY = m_latestOdomY;
                m_hasMapAnchor = true;
            }
        }

        if (m_mapReceived && m_hasMapAnchor && m_mapResolution > 0.0) {
            const double estimatedWorldX = m_mapAnchorWorldX + (m_latestOdomX - m_mapAnchorOdomX);
            const double estimatedWorldY = m_mapAnchorWorldY + (m_latestOdomY - m_mapAnchorOdomY);
            normalizedX = (estimatedWorldX - m_mapOriginX) / (m_mapResolution * static_cast<double>(m_mapWidth));
            normalizedY = 1.0 - ((estimatedWorldY - m_mapOriginY) / (m_mapResolution * static_cast<double>(m_mapHeight)));
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
        }

        if (onRobotPoseUpdated) {
            onRobotPoseUpdated(QPointF(std::clamp(normalizedX, 0.0, 1.0),
                                       std::clamp(normalizedY, 0.0, 1.0)),
                               headingDegrees);
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
    }

    rclcpp::Node::SharedPtr m_node;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr m_cmdVelPublisher;
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr m_mapSubscription;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr m_odomSubscription;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr m_scanSubscription;
    std::unique_ptr<tf2_ros::Buffer> m_tfBuffer;
    std::shared_ptr<tf2_ros::TransformListener> m_tfListener;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::thread m_spinThread;
    std::atomic<bool> m_running;
    bool m_mapReceived;
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
    double m_mapAnchorWorldX {0.0};
    double m_mapAnchorWorldY {0.0};
    double m_mapAnchorOdomX {0.0};
    double m_mapAnchorOdomY {0.0};
    std::chrono::steady_clock::time_point m_ignoreMapUntil;
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
    , m_startButton(nullptr)
    , m_saveButton(nullptr)
    , m_resetButton(nullptr)
    , m_slamProcess(new QProcess(this))
    , m_saveProcess(new QProcess(this))
    , m_manualDriveTimer(new QTimer(this))
    , m_mapBridge(std::make_unique<SlamMapBridge>())
    , m_mappingActive(false)
    , m_manualLinearX(0.0)
    , m_manualAngularZ(0.0)
{
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
    rightCol->addWidget(buildDirectionPad());
    rightCol->addWidget(buildSpeedLimitPanel(), 1);

    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(200);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    layout->addLayout(bodyRow, 1);

    connect(m_slamProcess, &QProcess::stateChanged, this, &SlamMappingPage::handleSlamProcessStateChanged);
    connect(m_slamProcess, &QProcess::errorOccurred, this, &SlamMappingPage::handleSlamProcessErrorOccurred);
    connect(m_saveProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &SlamMappingPage::handleSaveProcessFinished);
    connect(m_saveProcess, &QProcess::errorOccurred, this, &SlamMappingPage::handleSaveProcessErrorOccurred);
    m_manualDriveTimer->setInterval(100);
    connect(m_manualDriveTimer, &QTimer::timeout, this, &SlamMappingPage::handleManualDriveTick);

    m_mapBridge->onMapImageUpdated = [this](const QImage &image) {
        QMetaObject::invokeMethod(this, [this, image]() { handleMapImageUpdated(image); }, Qt::QueuedConnection);
    };
    m_mapBridge->onRobotPoseUpdated = [this](const QPointF &normalizedPos, qreal headingDegrees) {
        QMetaObject::invokeMethod(
            this,
            [this, normalizedPos, headingDegrees]() { handleRobotPoseUpdated(normalizedPos, headingDegrees); },
            Qt::QueuedConnection);
    };
    m_mapBridge->onStatusUpdated = [this](const QString &message) {
        QMetaObject::invokeMethod(this, [this, message]() { handleBridgeStatusUpdated(message); }, Qt::QueuedConnection);
    };
    updateSlamUiState();
}

SlamMappingPage::~SlamMappingPage()
{
    if (m_saveProcess->state() != QProcess::NotRunning) {
        m_saveProcess->kill();
        m_saveProcess->waitForFinished(1000);
    }
    if (m_slamProcess->state() != QProcess::NotRunning) {
        m_slamProcess->terminate();
        if (!m_slamProcess->waitForFinished(2000)) {
            m_slamProcess->kill();
            m_slamProcess->waitForFinished(1000);
        }
    }
    m_mapBridge->stop();
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

    connect(forwardButton, &QPushButton::clicked, this, [this]() {
        setManualDriveCommand(0.06, 0.0, QStringLiteral("수동 제어 · 전진 0.06 m/s"));
    });
    connect(backwardButton, &QPushButton::clicked, this, [this]() {
        setManualDriveCommand(-0.06, 0.0, QStringLiteral("수동 제어 · 후진 -0.06 m/s"));
    });
    connect(leftButton, &QPushButton::clicked, this, [this]() {
        setManualDriveCommand(0.0, 0.02, QStringLiteral("수동 제어 · 좌회전 0.02 rad/s"));
    });
    connect(rightButton, &QPushButton::clicked, this, [this]() {
        setManualDriveCommand(0.0, -0.02, QStringLiteral("수동 제어 · 우회전 -0.02 rad/s"));
    });
    connect(stopButton, &QPushButton::clicked, this, &SlamMappingPage::stopManualDrive);

    layout->addWidget(forwardButton, 0, 1);
    layout->addWidget(leftButton, 1, 0);
    layout->addWidget(stopButton, 1, 1);
    layout->addWidget(rightButton, 1, 2);
    layout->addWidget(backwardButton, 2, 1);
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
    auto *valueLabel = makeLabel(QStringLiteral("0.15 m/s"));
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Cyan.name()));
    topRow->addWidget(valueLabel);
    layout->addLayout(topRow);
    layout->addWidget(makeProportionalBar(50, DashboardPalette::Cyan, panel));
    layout->addStretch();
    return panel;
}

void SlamMappingPage::updateSlamUiState()
{
    const bool slamRunning = (m_slamProcess->state() == QProcess::Running);
    const bool slamStarting = (m_slamProcess->state() == QProcess::Starting);
    const bool saveRunning = (m_saveProcess->state() != QProcess::NotRunning);

    if (m_statusBannerTitleLabel != nullptr) {
        m_statusBannerTitleLabel->setText(slamRunning ? QStringLiteral("🗺  Cartographer 매핑 실행 중")
                                                      : (slamStarting ? QStringLiteral("🗺  Cartographer 시작 중")
                                                                      : QStringLiteral("🗺  Cartographer 대기 중")));
    }
    if (m_statusBannerDetailLabel != nullptr) {
        if (slamRunning) {
            m_statusBannerDetailLabel->setText(QStringLiteral("LiDAR, odom, tf가 들어오면 Cartographer가 실시간으로 지도를 확장합니다."));
        } else if (saveRunning) {
            m_statusBannerDetailLabel->setText(QStringLiteral("현재 점유격자를 maps 폴더에 저장하는 중입니다."));
        } else {
            m_statusBannerDetailLabel->setText(QStringLiteral("TurtleBot3 bringup 후 매핑 시작 버튼으로 Cartographer를 실행합니다."));
        }
    }
    if (m_mappingStatusValueLabel != nullptr) {
        const QString state = processStateText(m_slamProcess->state());
        const QColor color = slamRunning ? DashboardPalette::Green :
                             (slamStarting ? DashboardPalette::Cyan : DashboardPalette::Amber);
        m_mappingStatusValueLabel->setText(state);
        m_mappingStatusValueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(color.name()));
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
    stopMappingProcess();
    m_mapBridge->beginFreshSession();
    stopManualDrive();
    m_mappingActive = true;
    m_mapView->clearBackgroundImage();
    m_mapView->setPlannedPath({});
    m_mapView->setCaption(QStringLiteral("새 SLAM 세션 준비 중 · 기존 맵 무시 후 실시간 맵을 기다리는 중"));
    m_mapView->setRobotPose(QPointF(0.5, 0.5), 0.0);
    m_mapView->setRobotState(RobotMarkerState::Ready);
    const QString slamCommand = shellCommandPrefix()
        + QStringLiteral("ros2 launch turtlebot3_cartographer cartographer.launch.py use_sim_time:=false use_rviz:=false");
    m_slamProcess->setProgram(QStringLiteral("bash"));
    m_slamProcess->setArguments({
        QStringLiteral("-lc"),
        slamCommand
    });
    m_slamProcess->start();
    if (!m_slamProcess->waitForStarted(3000)) {
        updateSlamUiState();
        setStatusMessage(QStringLiteral("Cartographer 시작 실패: %1").arg(m_slamProcess->errorString()));
        return false;
    }
    m_mapBridge->start();
    m_mapView->setCaption(QStringLiteral("실시간 Cartographer 맵 수신 대기 중"));
    m_mapView->setRobotPose(QPointF(0.5, 0.5), 0.0);
    m_mapView->setRobotState(RobotMarkerState::Moving);
    updateSlamUiState();
    return true;
}

void SlamMappingPage::stopMappingProcess()
{
    stopManualDrive();
    m_mappingActive = false;
    m_mapBridge->stop();
    if (m_slamProcess->state() == QProcess::NotRunning) {
        return;
    }

    m_slamProcess->terminate();
    if (!m_slamProcess->waitForFinished(2000)) {
        m_slamProcess->kill();
        m_slamProcess->waitForFinished(1000);
    }
}

void SlamMappingPage::handleStartMapping()
{
    if (m_slamProcess->state() != QProcess::NotRunning) {
        setStatusMessage(QStringLiteral("Cartographer가 이미 실행 중입니다."));
        return;
    }

    if (!startMappingProcess()) {
        return;
    }
    setStatusMessage(QStringLiteral("새 Cartographer 세션을 시작했습니다. 기존 맵은 가져오지 않고 새 매핑만 표시합니다."));
}

void SlamMappingPage::handleResetMapping()
{
    stopMappingProcess();
    m_lastSavedBasePath.clear();
    m_mapBridge->beginFreshSession();
    m_mapView->clearBackgroundImage();
    m_mapView->setCaption(QStringLiteral("실시간 Cartographer 맵 수신 대기 중"));
    m_mapView->setRobotPose(QPointF(0.5, 0.5), 0.0);
    m_mapView->setRobotState(RobotMarkerState::Ready);
    m_mapView->setPlannedPath({});
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
    updateSlamUiState();
}

void SlamMappingPage::handleSlamProcessErrorOccurred()
{
    updateSlamUiState();
    setStatusMessage(QStringLiteral("Cartographer 실행 오류: %1").arg(m_slamProcess->errorString()));
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
    m_mapBridge->publishVelocity(m_manualLinearX, m_manualAngularZ);
}

void SlamMappingPage::handleMapImageUpdated(const QImage &image)
{
    if (!m_mappingActive) {
        return;
    }
    m_mapView->setBackgroundImage(image);
    m_mapView->setCaption(QStringLiteral("실시간 Cartographer 매핑 화면"));
}

void SlamMappingPage::handleRobotPoseUpdated(const QPointF &normalizedPos, qreal headingDegrees)
{
    if (!m_mappingActive) {
        return;
    }
    m_mapView->setRobotPose(normalizedPos, headingDegrees);
    m_mapView->setRobotState(RobotMarkerState::Moving);
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

void SlamMappingPage::setManualDriveCommand(double linearX, double angularZ, const QString &statusMessage)
{
    m_manualLinearX = linearX;
    m_manualAngularZ = angularZ;
    m_mapBridge->publishVelocity(linearX, angularZ);
    if (!qFuzzyIsNull(m_manualLinearX) || !qFuzzyIsNull(m_manualAngularZ)) {
        m_manualDriveTimer->start();
    } else {
        m_manualDriveTimer->stop();
    }

    setStatusMessage(statusMessage);
}

void SlamMappingPage::stopManualDrive()
{
    m_manualDriveTimer->stop();
    m_manualLinearX = 0.0;
    m_manualAngularZ = 0.0;
    m_mapBridge->publishVelocity(0.0, 0.0);
}

bool SlamMappingPage::consumeManualKey(int key)
{
    switch (key) {
    case Qt::Key_Up:
        setManualDriveCommand(0.06, 0.0, QStringLiteral("수동 제어 · 전진 0.06 m/s"));
        return true;
    case Qt::Key_Down:
        setManualDriveCommand(-0.06, 0.0, QStringLiteral("수동 제어 · 후진 -0.06 m/s"));
        return true;
    case Qt::Key_Left:
        setManualDriveCommand(0.0, 0.02, QStringLiteral("수동 제어 · 좌회전 0.02 rad/s"));
        return true;
    case Qt::Key_Right:
        setManualDriveCommand(0.0, -0.02, QStringLiteral("수동 제어 · 우회전 -0.02 rad/s"));
        return true;
    case Qt::Key_Space:
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
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
        return true;
    default:
        return false;
    }
}
