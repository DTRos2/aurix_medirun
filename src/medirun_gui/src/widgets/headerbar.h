#ifndef HEADERBAR_H
#define HEADERBAR_H

#include "../models/telemetrymodel.h"

#include <QFrame>

class QLabel;

class HeaderBar : public QFrame
{
    Q_OBJECT

public:
    explicit HeaderBar(QWidget *parent = nullptr);

    void setPageTitle(const QString &title);
    void applySnapshot(const TelemetrySnapshot &snapshot);

signals:
    void estopRequested();
    void logoutRequested();

private:
    QLabel *m_pageTitleLabel;
    QLabel *m_rosChip;
    QLabel *m_dbChip;
    QLabel *m_camChip;
    QLabel *m_batteryLabel;
};

#endif // HEADERBAR_H
