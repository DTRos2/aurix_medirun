#ifndef REMOTECONTROLPAGE_H
#define REMOTECONTROLPAGE_H

#include <QWidget>

class RemoteControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteControlPage(QWidget *parent = nullptr);

private:
    QWidget *buildManualControlBanner();
    QWidget *buildCameraPanel();
    QWidget *buildMinimapPanel();
    QWidget *buildDirectionPad();
    QWidget *buildSpeedLimitPanel();
};

#endif // REMOTECONTROLPAGE_H
