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

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRectF drawableArea() const;
    QRectF mapContentArea(const QRectF &area) const;
    QPointF toCanvas(const QPointF &normalized, const QRectF &area) const;
    void layoutOverlay();
    void updateScaledBackgroundPixmap();

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
