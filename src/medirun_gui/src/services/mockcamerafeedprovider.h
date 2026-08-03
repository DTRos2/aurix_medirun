#ifndef MOCKCAMERAFEEDPROVIDER_H
#define MOCKCAMERAFEEDPROVIDER_H

#include "camerafeedprovider.h"

class QTimer;

class MockCameraFeedProvider : public CameraFeedProvider
{
    Q_OBJECT

public:
    explicit MockCameraFeedProvider(QObject *parent = nullptr);

public slots:
    void start() override;

private slots:
    void publishNextFrame();

private:
    QImage buildFrame() const;

    QTimer *m_timer;
    int m_step;
};

#endif // MOCKCAMERAFEEDPROVIDER_H
