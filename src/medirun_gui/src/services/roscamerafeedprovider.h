#ifndef ROSCAMERAFEEDPROVIDER_H
#define ROSCAMERAFEEDPROVIDER_H

#include "camerafeedprovider.h"

#include <QTimer>

#include <atomic>
#include <memory>
#include <thread>

#include <image_transport/image_transport.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>

class RosCameraFeedProvider : public CameraFeedProvider
{
    Q_OBJECT

public:
    explicit RosCameraFeedProvider(QObject *parent = nullptr);
    ~RosCameraFeedProvider() override;

public slots:
    void start() override;

private slots:
    void checkFrameTimeout();

private:
    void stopExecutor();
    void handleImage(const sensor_msgs::msg::Image::ConstSharedPtr &msg);

    QTimer *m_timeoutTimer;
    rclcpp::Node::SharedPtr m_node;
    // image_transport::create_subscription으로 "compressed" transport hint를 줘서
    // /camera/image_raw/compressed(JPEG)를 BEST_EFFORT, depth=1로 구독한다. UDP에서
    // 손실된 과거 프레임을 기다리거나 쌓지 않고 항상 최신 프레임을 우선한다.
    image_transport::Subscriber m_subscription;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::thread m_spinThread;
    std::atomic<bool> m_started;
    std::atomic<bool> m_connected;
    std::atomic<qint64> m_lastFrameMs;
};

#endif // ROSCAMERAFEEDPROVIDER_H
