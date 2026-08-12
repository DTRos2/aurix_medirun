#ifndef SENSORTELEMETRYPROVIDER_H
#define SENSORTELEMETRYPROVIDER_H

#include <QObject>

// GUI-side abstraction over the Arduino 초음파/홀센서 토픽(/medirun/sensor/front_distance_cm,
// /medirun/sensor/payload_loaded)이라, 실시간 관제의 "로봇 상태" 패널이 rclcpp를 몰라도
// 되게 한다. Mirrors telemetryprovider.h/camerafeedprovider.h's mock/ros split.
class SensorTelemetryProvider : public QObject
{
    Q_OBJECT

public:
    explicit SensorTelemetryProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~SensorTelemetryProvider() override = default;

public slots:
    virtual void start() = 0;

signals:
    void frontDistanceChanged(int centimeters);
    void rearDistanceChanged(int centimeters);
    void payloadLoadedChanged(bool loaded);
    // 03번 설계서 1.5절 "통신" 규칙과 동일하게 일정 시간 미수신 시 단절로 본다.
    void connectionStateChanged(bool connected);
};

#endif // SENSORTELEMETRYPROVIDER_H
