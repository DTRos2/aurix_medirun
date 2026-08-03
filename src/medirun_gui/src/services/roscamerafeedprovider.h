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
    // /camera/image_raw/compressed(JPEG)를 구독 — 대역폭 절감 목적. v4l2_camera_node가
    // image_transport 퍼블리셔라 컴프레스드 토픽을 자동으로 같이 발행해준다.
    image_transport::Subscriber m_subscription;
    std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> m_executor;
    std::thread m_spinThread;
    std::atomic<bool> m_started;
    std::atomic<bool> m_connected;
    std::atomic<qint64> m_lastFrameMs;
};

#endif // ROSCAMERAFEEDPROVIDER_H
