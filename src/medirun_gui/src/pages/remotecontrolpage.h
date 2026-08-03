#ifndef REMOTECONTROLPAGE_H
#define REMOTECONTROLPAGE_H

#include "../services/mapfile.h"

#include <QImage>
#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;
class MapViewWidget;
class CameraFeedProvider;
class NavigationService;

class RemoteControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteControlPage(QWidget *parent = nullptr);

    void setCameraFeedProvider(CameraFeedProvider *provider);
    void setNavigationService(NavigationService *service);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void applyCameraFrame(const QImage &frame);
    void applyCameraConnectionState(bool connected);
    void handleAmclPoseUpdated(double x, double y, double yawRad);

private:
    QWidget *buildManualControlBanner();
    QWidget *buildCameraPanel();
    QWidget *buildMinimapPanel();
    QWidget *buildPoseHintBanner();
    QWidget *buildViewSwapButton();
    QWidget *buildLoadMapButton();
    QWidget *buildDirectionPad();
    QWidget *buildSpeedPresetPanel();
    QWidget *buildSpeedAxisRow(const QString &label, const QString &value, const QString &unit, int percent);
    QWidget *buildCmdVelPanel();

    void swapCameraAndMap();
    void handleLoadMap();
    void handleSetInitialPoseToggled(bool checked);
    void handlePosePickRequested(const QPointF &normalizedPos, double yawRadians);
    QPointF worldToNormalized(double x, double y) const;
    QPointF normalizedToWorld(const QPointF &normalized) const;
    void updateInitialPoseButtonEnabled();

    QWidget *m_cameraPanel = nullptr;
    QWidget *m_minimapPanel = nullptr;
    QLabel *m_cameraFrameLabel = nullptr;
    // 카메라 패널이 축소된(스왑된) 상태일 때는 감춰서 영상 표시 영역을 넓게 확보한다.
    QWidget *m_cameraTopRow = nullptr;
    QWidget *m_cameraBottomRow = nullptr;
    MapViewWidget *m_mapView = nullptr;
    // 지도가 축소된(미니맵) 상태일 때는 감춰서, 확대(메인) 상태일 때만 보인다.
    QWidget *m_mapPoseHintBanner = nullptr;
    QPushButton *m_setInitialPoseButton = nullptr;
    QVBoxLayout *m_bigSlot = nullptr;
    QVBoxLayout *m_smallSlot = nullptr;
    bool m_mapIsMain = false;

    CameraFeedProvider *m_cameraFeedProvider = nullptr;
    QImage m_lastCameraFrame;

    NavigationService *m_navigationService = nullptr;
    MapMeta m_currentMapMeta;
    bool m_settingInitialPose = false;
};

#endif // REMOTECONTROLPAGE_H
