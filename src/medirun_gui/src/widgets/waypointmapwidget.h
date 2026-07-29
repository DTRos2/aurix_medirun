#ifndef WAYPOINTMAPWIDGET_H
#define WAYPOINTMAPWIDGET_H

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QVector>

class QLabel;

enum class WaypointMarkerKind {
    CallZone,
    Waypoint,
    Home
};

struct WaypointMapMarker
{
    QString id;
    QString label;
    QPointF normalizedPos;
    WaypointMarkerKind kind {WaypointMarkerKind::Waypoint};
};

// Static marker map shared by the waypoint list/edit tab and the trajectory
// builder tab. Positions are normalized to [0,1] x [0,1] like MapViewWidget,
// but markers additionally carry an id/label and (for the trajectory tab) an
// optional route order so the same canvas can render either view.
class WaypointMapWidget : public QFrame
{
    Q_OBJECT

public:
    explicit WaypointMapWidget(QWidget *parent = nullptr);

    void setCaption(const QString &text);
    void setLegend(const QString &text);
    void setBackgroundImage(const QImage &image);
    void clearBackgroundImage();
    void setMarkers(const QVector<WaypointMapMarker> &markers);
    void setSelectedId(const QString &id);
    // Non-empty to render numbered route markers + connecting path instead
    // of the plain marker glyphs.
    void setRouteOrder(const QVector<QString> &orderedIds);

signals:
    void markerClicked(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QRectF drawableArea() const;
    QPointF toCanvas(const QPointF &normalized, const QRectF &area) const;
    void layoutOverlay();

    QVector<WaypointMapMarker> m_markers;
    QString m_selectedId;
    QVector<QString> m_routeOrder;
    QPixmap m_backgroundPixmap;

    QLabel *m_captionLabel;
    QLabel *m_legendLabel;
};

#endif // WAYPOINTMAPWIDGET_H
