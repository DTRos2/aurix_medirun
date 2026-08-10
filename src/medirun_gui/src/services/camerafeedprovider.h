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

    // 카메라가 로봇에 180도 회전된 방향(상하+좌우 모두 뒤집힌 상태)으로 장착돼
    // 있어 실제 눈으로 보는 방향과 일치시키려면 두 축을 모두 뒤집어야 한다.
    // 축 하나만 뒤집으면(예: mirrored(false, true)) 나머지 축이 거울상으로
    // 남아 좌우가 반전돼 보인다 — 실제로 그렇게 되어있던 버그였다. 구현체마다
    // 원시 mirrored(bool, bool) 호출을 반복하면 그 실수가 또 나기 쉬우므로,
    // 실물 카메라 프레임을 만드는 provider는 이 헬퍼를 통해서만 보정한다.
    static QImage correctMountOrientation(const QImage &frame)
    {
        return frame.mirrored(true, true);
    }

public slots:
    virtual void start() = 0;

signals:
    void frameReady(const QImage &frame);
    // 03번 설계서 1.5절 "통신" 규칙: 2초 미수신 시 단절로 본다.
    void connectionStateChanged(bool connected);
};

#endif // CAMERAFEEDPROVIDER_H
