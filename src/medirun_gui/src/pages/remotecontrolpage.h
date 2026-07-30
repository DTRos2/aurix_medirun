#ifndef REMOTECONTROLPAGE_H
#define REMOTECONTROLPAGE_H

#include <QWidget>

class QVBoxLayout;
class MapViewWidget;

class RemoteControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteControlPage(QWidget *parent = nullptr);

private:
    QWidget *buildManualControlBanner();
    QWidget *buildCameraPanel();
    QWidget *buildMinimapPanel();
    QWidget *buildViewSwapButton();
    QWidget *buildDirectionPad();
    QWidget *buildSpeedPresetPanel();
    QWidget *buildSpeedAxisRow(const QString &label, const QString &value, const QString &unit, int percent);
    QWidget *buildCmdVelPanel();

    void swapCameraAndMap();

    QWidget *m_cameraPanel = nullptr;
    QWidget *m_minimapPanel = nullptr;
    MapViewWidget *m_mapView = nullptr;
    QVBoxLayout *m_bigSlot = nullptr;
    QVBoxLayout *m_smallSlot = nullptr;
    bool m_mapIsMain = false;
};

#endif // REMOTECONTROLPAGE_H
