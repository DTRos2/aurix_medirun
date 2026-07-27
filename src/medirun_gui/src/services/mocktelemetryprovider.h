#ifndef MOCKTELEMETRYPROVIDER_H
#define MOCKTELEMETRYPROVIDER_H

#include "telemetryprovider.h"

class QTimer;

class MockTelemetryProvider : public TelemetryProvider
{
    Q_OBJECT

public:
    explicit MockTelemetryProvider(QObject *parent = nullptr);

public slots:
    void start() override;

private slots:
    void publishNextSnapshot();

private:
    TelemetrySnapshot buildSnapshot() const;
    SystemEventItem buildEvent(const QString &type, const QString &message, SeverityLevel severity) const;

    QTimer *m_timer;
    int m_step;
};

#endif // MOCKTELEMETRYPROVIDER_H
