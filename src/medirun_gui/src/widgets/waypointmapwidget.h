#ifndef WAYPOINTMAPWIDGET_H
#define WAYPOINTMAPWIDGET_H

#include "../models/waypointtypes.h"
#include "../services/mapfile.h"

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QVector>

class QLabel;

// Static marker map shared by the waypoint list/edit tab and the trajectory
// builder tab. Marker positions are normalized to the *loaded map image's*
// pixel space ([0,1] x [0,1], u=col/width, v=row/height) rather than to the
// widget's drawable area, so overlays stay aligned with the geo-referenced
// map regardless of window size or the image's aspect-ratio letterboxing.
//
// Besides rendering, this widget recognizes a click(-and-drag) gesture on
// empty map area to let the page create a new waypoint at the picked
// location: press+release near an existing marker selects it (markerClicked,
// unchanged); press+drag+release on empty space reports the picked point and
// a yaw derived from the drag direction (waypointPickRequested), mirroring
// the pyqt reference dashboard's map_panel click/drag-to-place UX.
class WaypointMapWidget : public QFrame
{
    Q_OBJECT

public:
    explicit WaypointMapWidget(QWidget *parent = nullptr);

    void setCaption(const QString &text);
    void setLegend(const QString &text);
    void setBackgroundImage(const QImage &image);
    void clearBackgroundImage();
    // meta.valid == false clears any stored geo-reference (pixelToWorld/
    // worldToPixel become unavailable to callers, who must check validity).
    void setMapMeta(const MapMeta &meta);
    const MapMeta &mapMeta() const { return m_mapMeta; }

    void setMarkers(const QVector<WaypointMapMarker> &markers);
    void setSelectedId(const QString &id);
    // Non-empty to render numbered route markers + connecting path instead
    // of the plain marker glyphs.
    void setRouteOrder(const QVector<QString> &orderedIds);
    // 로봇이 지금 실제로 navigateToPose/followWaypoints로 향하고 있는 웨이포인트
    // id — setSelectedId()(단순 편집용 선택)와 별개로, 로봇 위치에서 이 마커까지
    // 안내선을 긋고 눈에 띄는 목표 링을 덧그린다. 빈 문자열이면 표시 안 함.
    void setNavigationTargetId(const QString &id);

    // Robot's live pose (AMCL), normalized to the same image-uv space as
    // markers. valid=false hides the marker (e.g. AMCL not running yet).
    void setRobotPose(const QPointF &normalizedPos, double yawRadians, bool valid);

    // Live LaserScan points (already map-frame -> normalized by the page),
    // drawn as small dots so obstacle returns can be visually checked
    // against the map's walls (RViz LaserScan-display equivalent). Pass an
    // empty vector to clear (e.g. when the map/robot pose becomes invalid).
    void setScanPoints(const QVector<QPointF> &normalizedPoints);

    // 초기 위치 설정 모드(RViz 2D Pose Estimate와 동일)에서는 마커 위를
    // 클릭해도 그 마커를 선택하지 말고 그 지점을 그대로 pose pick으로
    // 취급해야 한다 — 마커 위는 클릭이 안 먹는 것처럼 보이던 문제 수정
    // (2026-08-19). true인 동안은 mousePressEvent가 마커 히트테스트를
    // 아예 건너뛴다.
    void setPoseSettingEnabled(bool enabled);

signals:
    void markerClicked(const QString &id);
    void waypointPickRequested(const QPointF &normalizedPos, double yawRadians);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF drawableArea() const;
    // Rect (in widget coordinates) the background image is actually drawn
    // into: the full drawable area if no image is loaded, otherwise the
    // KeepAspectRatio-scaled + centered rect used by paintEvent.
    QRectF imageRect() const;
    QPointF toCanvas(const QPointF &normalized, const QRectF &area) const;
    QPointF canvasToNormalized(const QPointF &canvasPos, const QRectF &area) const;
    void layoutOverlay();

    QVector<WaypointMapMarker> m_markers;
    QString m_selectedId;
    QString m_navigationTargetId;
    QVector<QString> m_routeOrder;
    QPixmap m_backgroundPixmap;
    MapMeta m_mapMeta;

    bool m_hasRobotPose {false};
    QPointF m_robotNormalizedPos;
    double m_robotYawRad {0.0};

    QVector<QPointF> m_scanNormalizedPoints;

    bool m_poseSettingEnabled {false};
    bool m_dragging {false};
    QPointF m_dragStartPos;
    QPointF m_dragCurrentPos;
    QString m_pressedMarkerId;

    QLabel *m_captionLabel;
    QLabel *m_legendLabel;
};

#endif // WAYPOINTMAPWIDGET_H
