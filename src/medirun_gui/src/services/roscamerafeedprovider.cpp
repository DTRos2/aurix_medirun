#include "roscamerafeedprovider.h"

#include <cv_bridge/cv_bridge.h>

#include <QDateTime>
#include <QImage>

namespace {
constexpr int kFrameTimeoutMs = 2000; // 03번 설계서 1.5절 "통신" 규칙: 2초 미수신 시 단절
constexpr int kTimeoutCheckIntervalMs = 500;

qint64 currentTimeMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}
} // namespace

RosCameraFeedProvider::RosCameraFeedProvider(QObject *parent)
    : CameraFeedProvider(parent)
    , m_timeoutTimer(new QTimer(this))
    , m_node(std::make_shared<rclcpp::Node>("medirun_gui_camera_feed"))
    , m_subscription()
    , m_executor(std::make_unique<rclcpp::executors::SingleThreadedExecutor>())
    , m_started(false)
    , m_connected(false)
    , m_lastFrameMs(0)
{
    m_timeoutTimer->setInterval(kTimeoutCheckIntervalMs);
    connect(m_timeoutTimer, &QTimer::timeout, this, &RosCameraFeedProvider::checkFrameTimeout);

    m_subscription = image_transport::create_subscription(
        m_node.get(),
        "camera/image_raw",
        [this](const sensor_msgs::msg::Image::ConstSharedPtr &msg) {
            handleImage(msg);
        },
        "compressed",
        rmw_qos_profile_sensor_data);
}

RosCameraFeedProvider::~RosCameraFeedProvider()
{
    stopExecutor();
}

void RosCameraFeedProvider::start()
{
    if (m_started.exchange(true)) {
        return;
    }

    m_executor->add_node(m_node);
    m_spinThread = std::thread([this]() {
        m_executor->spin();
    });

    m_timeoutTimer->start();
}

void RosCameraFeedProvider::checkFrameTimeout()
{
    if (m_lastFrameMs.load() == 0) {
        return;
    }

    const qint64 elapsedMs = currentTimeMs() - m_lastFrameMs.load();
    const bool stillConnected = elapsedMs < kFrameTimeoutMs;
    if (m_connected.exchange(stillConnected) != stillConnected) {
        emit connectionStateChanged(stillConnected);
    }
}

void RosCameraFeedProvider::stopExecutor()
{
    if (!m_started.exchange(false)) {
        return;
    }

    m_timeoutTimer->stop();

    if (m_executor != nullptr) {
        m_executor->cancel();
    }

    if (m_spinThread.joinable()) {
        m_spinThread.join();
    }

    if (m_executor != nullptr && m_node != nullptr) {
        m_executor->remove_node(m_node);
    }
}

void RosCameraFeedProvider::handleImage(const sensor_msgs::msg::Image::ConstSharedPtr &msg)
{
    m_lastFrameMs.store(currentTimeMs());
    if (!m_connected.exchange(true)) {
        emit connectionStateChanged(true);
    }

    cv_bridge::CvImageConstPtr cvImage;
    try {
        cvImage = cv_bridge::toCvShare(msg, "rgb8");
    } catch (const cv_bridge::Exception &) {
        return;
    }

    const QImage frame(cvImage->image.data,
                        cvImage->image.cols,
                        cvImage->image.rows,
                        static_cast<int>(cvImage->image.step),
                        QImage::Format_RGB888);
    // correctMountOrientation()이 새 QImage를 만들어 반환하므로 cvImage 버퍼에
    // 대한 깊은 복사도 겸함(mirrored() 기반).
    emit frameReady(correctMountOrientation(frame));
}
