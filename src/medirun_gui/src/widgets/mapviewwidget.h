#ifndef MAPVIEWWIDGET_H
#define MAPVIEWWIDGET_H

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QSize>
#include <QVector>

class QLabel;
class QPushButton;
class QMouseEvent;

enum class MapViewMode {
    Live,
    Playback,
    Reference
};

enum class RobotMarkerState {
    Ready,
    Moving,
    Arrived,
    Warning,
    ErrorStopped
};

// Reusable map canvas shared by LiveControlPage, RecordsPlaybackPage and
// RemoteControlPage's minimap. All positions are normalized to [0,1] x [0,1]
// so the same widget renders correctly at any size.
class MapViewWidget : public QFrame
{
    Q_OBJECT

public:
    explicit MapViewWidget(QWidget *parent = nullptr);

    void setMode(MapViewMode mode);
    void setCaption(const QString &text);
    void setRecording(bool recording);
    void setOverlayChipsVisible(bool visible);

    // nav2 map_server YAML에서 불러온 배경 지도 이미지. 없으면(비어있으면)
    // 지금까지처럼 점선 테두리만 있는 빈 캔버스로 그려진다.
    void setBackgroundImage(const QImage &image);
    void clearBackgroundImage();
    void setRobotVisible(bool visible);

    void setRobotPose(const QPointF &normalizedPos, qreal headingDegrees);
    void setRobotState(RobotMarkerState state);

    // Live mode: single dashed cyan "planned route" segment.
    void setPlannedPath(const QVector<QPointF> &normalizedPoints);
    // Playback mode: solid cyan "traveled" + dashed muted "remaining".
    void setPlaybackPath(const QVector<QPointF> &traveled, const QVector<QPointF> &remaining);

    void setWaypoints(const QVector<QPointF> &normalizedPoints);
    void setDestination(const QPointF &normalizedPos, bool visible = true);
    void setHome(const QPointF &normalizedPos, bool visible = true);

    // 지도 클릭+드래그로 초기 위치(AMCL 2D Pose Estimate)를 지정하는 제스처를
    // 켠다/끈다. 기본은 꺼짐 — LiveControlPage/RecordsPlaybackPage처럼 보기
    // 전용으로 쓰는 페이지는 켤 필요가 없어서 옵트인으로 둔다.
    void setPoseSettingEnabled(bool enabled);

signals:
    void posePickRequested(const QPointF &normalizedPos, double yawRadians);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF drawableArea() const;
    // 배경 이미지가 있으면 drawableArea() 안에서 종횡비 유지로 축소/중앙정렬된
    // 사각형, 없으면 drawableArea()와 동일. 마커/경로/로봇 위치는 전부 이
    // 사각형 기준으로 그려진다 (WaypointMapWidget과 동일한 패턴).
    QRectF imageRect() const;
    QPointF toCanvas(const QPointF &normalized, const QRectF &area) const;
    QPointF canvasToNormalized(const QPointF &canvasPos, const QRectF &area) const;
    void layoutOverlay();
    void updateScaledBackgroundPixmap();

    bool m_poseSettingEnabled;
    bool m_dragging;
    QPointF m_dragStartPos;
    QPointF m_dragCurrentPos;
    MapViewMode m_mode;
    RobotMarkerState m_robotState;
    QPointF m_robotPos;
    qreal m_robotHeadingDeg;
    QVector<QPointF> m_solidPath;
    QVector<QPointF> m_dashedPath;
    QColor m_dashedPathColor;

    QImage m_originalBackgroundImage;
    QPixmap m_backgroundPixmap;
    QSize m_backgroundPixmapTargetSize;

    QVector<QPointF> m_waypoints;
    QPointF m_destination;
    bool m_hasDestination;
    QPointF m_home;
    bool m_hasHome;
    bool m_robotVisible;
    bool m_recording;
    bool m_overlayChipsVisible;

    QLabel *m_captionLabel;
    QLabel *m_recBadge;
    QPushButton *m_followChip;
    QPushButton *m_hidePathChip;
};

#endif // MAPVIEWWIDGET_H
