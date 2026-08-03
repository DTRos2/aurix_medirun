#include "mockcamerafeedprovider.h"

#include <QDateTime>
#include <QPainter>
#include <QTimer>

namespace {
constexpr int kFrameIntervalMs = 100; // ~10 FPS, --mock 개발용 표시일 뿐 실측 FPS 아님
constexpr int kFrameWidth = 640;
constexpr int kFrameHeight = 480;
}

MockCameraFeedProvider::MockCameraFeedProvider(QObject *parent)
    : CameraFeedProvider(parent)
    , m_timer(new QTimer(this))
    , m_step(0)
{
    m_timer->setInterval(kFrameIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &MockCameraFeedProvider::publishNextFrame);
}

void MockCameraFeedProvider::start()
{
    emit connectionStateChanged(true);
    publishNextFrame();
    m_timer->start();
}

void MockCameraFeedProvider::publishNextFrame()
{
    emit frameReady(buildFrame());
    ++m_step;
}

QImage MockCameraFeedProvider::buildFrame() const
{
    QImage image(kFrameWidth, kFrameHeight, QImage::Format_RGB888);
    image.fill(QColor(20, 28, 38));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    const int barX = (m_step * 6) % kFrameWidth;
    painter.fillRect(barX, 0, 8, kFrameHeight, QColor(0, 200, 220, 120));

    painter.setPen(QColor(150, 220, 230));
    painter.drawText(image.rect(), Qt::AlignCenter, QStringLiteral("MOCK CAMERA · %1").arg(m_step));

    return image;
}
