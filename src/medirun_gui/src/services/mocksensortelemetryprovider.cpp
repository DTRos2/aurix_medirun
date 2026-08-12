#include "mocksensortelemetryprovider.h"

#include <QTimer>

#include <cmath>

namespace {
constexpr int kIntervalMs = 500;
}

MockSensorTelemetryProvider::MockSensorTelemetryProvider(QObject *parent)
    : SensorTelemetryProvider(parent)
    , m_timer(new QTimer(this))
    , m_step(0)
{
    m_timer->setInterval(kIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &MockSensorTelemetryProvider::publishNextReading);
}

void MockSensorTelemetryProvider::start()
{
    emit connectionStateChanged(true);
    emit payloadLoadedChanged(true);
    publishNextReading();
    m_timer->start();
}

void MockSensorTelemetryProvider::publishNextReading()
{
    // 40~60cm를 오가는 값으로 살아있는 느낌만 준다 — 실측값 아님.
    const int frontDistance = 50 + static_cast<int>(10.0 * std::sin(m_step * 0.2));
    const int rearDistance = 80 + static_cast<int>(15.0 * std::sin(m_step * 0.15 + 1.0));
    emit frontDistanceChanged(frontDistance);
    emit rearDistanceChanged(rearDistance);
    ++m_step;
}
