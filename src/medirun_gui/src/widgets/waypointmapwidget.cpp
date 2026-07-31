#include "waypointmapwidget.h"

#include "../theme/dashboardpalette.h"

#include <QFontMetrics>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kMarkerHitRadius = 14.0;
constexpr qreal kClickVsDragPx = 5.0;
constexpr qreal kArrowLengthPx = 14.0;
constexpr qreal kRobotMarkerRadiusPx = 6.0;
constexpr qreal kRobotArrowLengthPx = 18.0;
constexpr qreal kScanPointRadiusPx = 1.4;

void paintMarkerGlyph(QPainter &painter, const QPointF &center, WaypointMarkerKind kind, bool selected)
{
    if (selected) {
        painter.setPen(QPen(DashboardPalette::Cyan, 1.5, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, 17, 17);
        painter.setPen(Qt::NoPen);
        painter.setBrush(DashboardPalette::Cyan);
        painter.drawEllipse(center, 8, 8);
        return;
    }

    switch (kind) {
    case WaypointMarkerKind::CallZone:
        painter.setPen(QPen(DashboardPalette::TextPrimary, 1.8));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, 9, 9);
        painter.setPen(Qt::NoPen);
        painter.setBrush(DashboardPalette::TextPrimary);
        painter.drawEllipse(center, 3, 3);
        break;
    case WaypointMarkerKind::Waypoint:
        painter.setPen(QPen(DashboardPalette::TextMuted, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, 7, 7);
        break;
    case WaypointMarkerKind::Home:
        painter.setPen(QPen(DashboardPalette::TextMuted, 1.3));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, 8, 8);
        painter.drawLine(QPointF(center.x() - 5, center.y() + 2), QPointF(center.x(), center.y() - 4));
        painter.drawLine(QPointF(center.x(), center.y() - 4), QPointF(center.x() + 5, center.y() + 2));
        break;
    }
}
} // namespace

WaypointMapWidget::WaypointMapWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("mapViewCanvas"));
    setMinimumSize(160, 90);

    m_captionLabel = new QLabel(this);
    m_captionLabel->setObjectName(QStringLiteral("waypointMapOverlayText"));
    m_captionLabel->hide();

    m_legendLabel = new QLabel(this);
    m_legendLabel->setObjectName(QStringLiteral("waypointMapOverlayText"));
    m_legendLabel->hide();
}

void WaypointMapWidget::setCaption(const QString &text)
{
    m_captionLabel->setText(text);
    m_captionLabel->setVisible(!text.isEmpty());
    layoutOverlay();
}

void WaypointMapWidget::setLegend(const QString &text)
{
    m_legendLabel->setText(text);
    m_legendLabel->setVisible(!text.isEmpty());
    layoutOverlay();
}

void WaypointMapWidget::setBackgroundImage(const QImage &image)
{
    m_backgroundPixmap = QPixmap::fromImage(image);
    update();
}

void WaypointMapWidget::clearBackgroundImage()
{
    m_backgroundPixmap = QPixmap();
    m_mapMeta = MapMeta();
    update();
}

void WaypointMapWidget::setMapMeta(const MapMeta &meta)
{
    m_mapMeta = meta;
}

void WaypointMapWidget::setMarkers(const QVector<WaypointMapMarker> &markers)
{
    m_markers = markers;
    update();
}

void WaypointMapWidget::setSelectedId(const QString &id)
{
    m_selectedId = id;
    update();
}

void WaypointMapWidget::setRouteOrder(const QVector<QString> &orderedIds)
{
    m_routeOrder = orderedIds;
    update();
}

void WaypointMapWidget::setRobotPose(const QPointF &normalizedPos, double yawRadians, bool valid)
{
    m_hasRobotPose = valid;
    m_robotNormalizedPos = normalizedPos;
    m_robotYawRad = yawRadians;
    update();
}

void WaypointMapWidget::setScanPoints(const QVector<QPointF> &normalizedPoints)
{
    m_scanNormalizedPoints = normalizedPoints;
    update();
}

QRectF WaypointMapWidget::drawableArea() const
{
    const qreal margin = 10.0;
    return QRectF(margin, margin, width() - margin * 2, height() - margin * 2);
}

QRectF WaypointMapWidget::imageRect() const
{
    const QRectF area = drawableArea();
    if (m_backgroundPixmap.isNull()) {
        return area;
    }

    const QSizeF scaled = m_backgroundPixmap.size().scaled(area.size().toSize(), Qt::KeepAspectRatio);
    const QPointF topLeft(area.left() + (area.width() - scaled.width()) / 2.0,
                           area.top() + (area.height() - scaled.height()) / 2.0);
    return QRectF(topLeft, scaled);
}

QPointF WaypointMapWidget::toCanvas(const QPointF &normalized, const QRectF &area) const
{
    return QPointF(area.left() + normalized.x() * area.width(),
                    area.top() + normalized.y() * area.height());
}

QPointF WaypointMapWidget::canvasToNormalized(const QPointF &canvasPos, const QRectF &area) const
{
    if (area.width() <= 0.0 || area.height() <= 0.0) {
        return QPointF(0.0, 0.0);
    }
    const qreal u = (canvasPos.x() - area.left()) / area.width();
    const qreal v = (canvasPos.y() - area.top()) / area.height();
    return QPointF(std::clamp(u, 0.0, 1.0), std::clamp(v, 0.0, 1.0));
}

void WaypointMapWidget::layoutOverlay()
{
    m_captionLabel->adjustSize();
    m_captionLabel->move(9, 7);

    m_legendLabel->adjustSize();
    m_legendLabel->move(9, height() - m_legendLabel->height() - 7);
}

void WaypointMapWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    layoutOverlay();
}

void WaypointMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QFrame::mousePressEvent(event);
        return;
    }

    const QRectF area = imageRect();
    m_pressedMarkerId.clear();
    for (const WaypointMapMarker &marker : m_markers) {
        const QPointF center = toCanvas(marker.normalizedPos, area);
        const QPointF delta = center - event->position();
        if (QPointF::dotProduct(delta, delta) <= kMarkerHitRadius * kMarkerHitRadius) {
            m_pressedMarkerId = marker.id;
            break;
        }
    }

    m_dragging = true;
    m_dragStartPos = event->position();
    m_dragCurrentPos = m_dragStartPos;
    update();
}

void WaypointMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    m_dragCurrentPos = event->position();
    update();
}

void WaypointMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QFrame::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    const qreal dxPx = m_dragCurrentPos.x() - m_dragStartPos.x();
    const qreal dyPx = m_dragCurrentPos.y() - m_dragStartPos.y();
    const qreal dragDistance = std::hypot(dxPx, dyPx);

    if (!m_pressedMarkerId.isEmpty()) {
        if (dragDistance < kClickVsDragPx) {
            emit markerClicked(m_pressedMarkerId);
        }
    } else {
        // 이미지 행(row) 축은 world y축과 반대 방향이므로 dy를 뒤집는다.
        const double yaw = dragDistance < kClickVsDragPx ? 0.0 : std::atan2(-dyPx, dxPx);
        const QPointF normalized = canvasToNormalized(m_dragStartPos, imageRect());
        emit waypointPickRequested(normalized, yaw);
    }

    m_pressedMarkerId.clear();
    update();
}

void WaypointMapWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = drawableArea();
    const QRectF imgRect = imageRect();

    if (!m_backgroundPixmap.isNull()) {
        const QPixmap scaled = m_backgroundPixmap.scaled(imgRect.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(imgRect.topLeft(), scaled);
    }

    painter.setPen(QPen(DashboardPalette::BorderGuide, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(area);

    painter.setPen(QPen(DashboardPalette::BorderDivider, 1));
    painter.drawLine(toCanvas(QPointF(0.0, 0.5), area), toCanvas(QPointF(1.0, 0.5), area));
    painter.drawLine(toCanvas(QPointF(0.5, 0.0), area), toCanvas(QPointF(0.5, 1.0), area));

    if (m_dragging && m_pressedMarkerId.isEmpty()) {
        painter.setPen(QPen(DashboardPalette::Cyan, 1, Qt::DashLine));
        painter.drawLine(m_dragStartPos, m_dragCurrentPos);
        painter.setPen(Qt::NoPen);
        painter.setBrush(DashboardPalette::Cyan);
        painter.drawEllipse(m_dragStartPos, 3, 3);
    }

    if (!m_routeOrder.isEmpty()) {
        QHash<QString, WaypointMapMarker> byId;
        for (const WaypointMapMarker &marker : m_markers) {
            byId.insert(marker.id, marker);
        }

        QVector<QPointF> orderedPoints;
        for (const QString &id : m_routeOrder) {
            if (byId.contains(id)) {
                orderedPoints.append(toCanvas(byId.value(id).normalizedPos, imgRect));
            }
        }

        if (orderedPoints.size() >= 2) {
            QPainterPath path;
            path.moveTo(orderedPoints.first());
            for (int i = 1; i < orderedPoints.size(); ++i) {
                path.lineTo(orderedPoints.at(i));
            }
            painter.setPen(QPen(DashboardPalette::Cyan, 2.5));
            painter.drawPath(path);
        }

        for (const WaypointMapMarker &marker : m_markers) {
            const QPointF center = toCanvas(marker.normalizedPos, imgRect);
            const int order = m_routeOrder.indexOf(marker.id);
            if (order < 0) {
                paintMarkerGlyph(painter, center, marker.kind, false);
                continue;
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(DashboardPalette::Cyan);
            painter.drawEllipse(center, 11, 11);
            painter.setPen(QPen(DashboardPalette::CyanOn));
            QFont numberFont = painter.font();
            numberFont.setPixelSize(10);
            numberFont.setBold(true);
            painter.setFont(numberFont);
            painter.drawText(QRectF(center.x() - 11, center.y() - 11, 22, 22), Qt::AlignCenter,
                              QString::number(order + 1));

            QFont labelFont = painter.font();
            labelFont.setPixelSize(9);
            labelFont.setBold(false);
            painter.setFont(labelFont);
            painter.setPen(DashboardPalette::TextSecondary);
            painter.drawText(QRectF(center.x() - 30, center.y() - 27, 60, 12), Qt::AlignCenter, marker.label);
        }
    } else {
        for (const WaypointMapMarker &marker : m_markers) {
            const QPointF center = toCanvas(marker.normalizedPos, imgRect);
            const bool selected = (marker.id == m_selectedId);
            paintMarkerGlyph(painter, center, marker.kind, selected);

            QFont labelFont = painter.font();
            labelFont.setPixelSize(9);
            painter.setFont(labelFont);
            painter.setPen(selected ? DashboardPalette::Cyan : DashboardPalette::TextSecondary);
            const QString text = selected ? QStringLiteral("%1 (편집 중)").arg(marker.label) : marker.label;
            const QFontMetrics metrics(labelFont);
            const int textWidth = metrics.horizontalAdvance(text) + 12;
            painter.drawText(QRectF(center.x() - textWidth / 2.0, center.y() - (selected ? 30 : 24), textWidth, 12),
                              Qt::AlignCenter, text);
        }
    }

    if (!m_scanNormalizedPoints.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(DashboardPalette::Red);
        for (const QPointF &normalized : m_scanNormalizedPoints) {
            painter.drawEllipse(toCanvas(normalized, imgRect), kScanPointRadiusPx, kScanPointRadiusPx);
        }
    }

    if (m_hasRobotPose) {
        const QPointF center = toCanvas(m_robotNormalizedPos, imgRect);
        const double dx = std::cos(m_robotYawRad) * kRobotArrowLengthPx;
        const double dy = -std::sin(m_robotYawRad) * kRobotArrowLengthPx;
        painter.setPen(QPen(DashboardPalette::Green, 2));
        painter.setBrush(DashboardPalette::Green);
        painter.drawEllipse(center, kRobotMarkerRadiusPx, kRobotMarkerRadiusPx);
        painter.drawLine(center, QPointF(center.x() + dx, center.y() + dy));
    }
}
