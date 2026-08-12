#ifndef MOCKSENSORTELEMETRYPROVIDER_H
#define MOCKSENSORTELEMETRYPROVIDER_H

#include "sensortelemetryprovider.h"

class QTimer;

class MockSensorTelemetryProvider : public SensorTelemetryProvider
{
    Q_OBJECT

public:
    explicit MockSensorTelemetryProvider(QObject *parent = nullptr);

public slots:
    void start() override;

private slots:
    void publishNextReading();

private:
    QTimer *m_timer;
    int m_step;
};

#endif // MOCKSENSORTELEMETRYPROVIDER_H
