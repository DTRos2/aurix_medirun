#ifndef CAMERAFEEDPROVIDER_H
#define CAMERAFEEDPROVIDER_H

#include <QImage>
#include <QObject>

// GUI-side abstraction over the camera feed so pages (remotecontrolpage) never
// need to include rclcpp/cv_bridge headers. Mirrors telemetryprovider.h's
// mock/ros split.
class CameraFeedProvider : public QObject
{
    Q_OBJECT

public:
    explicit CameraFeedProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~CameraFeedProvider() override = default;

public slots:
    virtual void start() = 0;

signals:
    void frameReady(const QImage &frame);
    // 03번 설계서 1.5절 "통신" 규칙: 2초 미수신 시 단절로 본다.
    void connectionStateChanged(bool connected);
};

#endif // CAMERAFEEDPROVIDER_H
