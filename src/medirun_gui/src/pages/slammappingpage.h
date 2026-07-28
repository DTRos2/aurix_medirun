#ifndef SLAMMAPPINGPAGE_H
#define SLAMMAPPINGPAGE_H

#include <QWidget>

class MapViewWidget;

class SlamMappingPage : public QWidget
{
    Q_OBJECT

public:
    explicit SlamMappingPage(QWidget *parent = nullptr);

private:
    QWidget *buildStatusBanner();
    QWidget *buildActionRow();
    QWidget *buildMappingStatusPanel();
    QWidget *buildDirectionPad();
    QWidget *buildSpeedLimitPanel();

    MapViewWidget *m_mapView;
};

#endif // SLAMMAPPINGPAGE_H
