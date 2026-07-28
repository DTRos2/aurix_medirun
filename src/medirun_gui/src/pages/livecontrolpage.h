#ifndef LIVECONTROLPAGE_H
#define LIVECONTROLPAGE_H

#include <QWidget>

class MapViewWidget;

class LiveControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit LiveControlPage(QWidget *parent = nullptr);

signals:
    void missionCancelRequested();
    void returnHomeRequested();

private:
    QWidget *buildMissionStepper();
    QWidget *buildActionRow();
    QWidget *buildRobotStatusPanel();
    QWidget *buildEventStreamPanel();
    QWidget *buildWaitingCallBanner();

    MapViewWidget *m_mapView;
};

#endif // LIVECONTROLPAGE_H
