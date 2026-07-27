#ifndef TELEMETRYPROVIDER_H
#define TELEMETRYPROVIDER_H

#include "../models/telemetrymodel.h"

#include <QObject>

class TelemetryProvider : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryProvider(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~TelemetryProvider() override = default;

public slots:
    virtual void start() = 0;

signals:
    void snapshotUpdated(const TelemetrySnapshot &snapshot);
};

#endif // TELEMETRYPROVIDER_H
