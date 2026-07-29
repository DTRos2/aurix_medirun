#include "waypointmapwidget.h"

#include "../theme/dashboardpalette.h"

#include <QFontMetrics>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr qreal kMarkerHitRadius = 14.0;

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
    update();
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

QRectF WaypointMapWidget::drawableArea() const
{
    const qreal margin = 10.0;
    return QRectF(margin, margin, width() - margin * 2, height() - margin * 2);
}

QPointF WaypointMapWidget::toCanvas(const QPointF &normalized, const QRectF &area) const
{
    return QPointF(area.left() + normalized.x() * area.width(),
                    area.top() + normalized.y() * area.height());
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
    const QRectF area = drawableArea();
    for (const WaypointMapMarker &marker : m_markers) {
        const QPointF center = toCanvas(marker.normalizedPos, area);
        const QPointF delta = center - event->pos();
        if (QPointF::dotProduct(delta, delta) <= kMarkerHitRadius * kMarkerHitRadius) {
            emit markerClicked(marker.id);
            return;
        }
    }
    QFrame::mousePressEvent(event);
}

void WaypointMapWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = drawableArea();

    if (!m_backgroundPixmap.isNull()) {
        const QPixmap scaled = m_backgroundPixmap.scaled(area.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPointF topLeft(area.left() + (area.width() - scaled.width()) / 2.0,
                               area.top() + (area.height() - scaled.height()) / 2.0);
        painter.drawPixmap(topLeft, scaled);
    }

    painter.setPen(QPen(DashboardPalette::BorderGuide, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(area);

    painter.setPen(QPen(DashboardPalette::BorderDivider, 1));
    painter.drawLine(toCanvas(QPointF(0.0, 0.5), area), toCanvas(QPointF(1.0, 0.5), area));
    painter.drawLine(toCanvas(QPointF(0.5, 0.0), area), toCanvas(QPointF(0.5, 1.0), area));

    if (!m_routeOrder.isEmpty()) {
        QHash<QString, WaypointMapMarker> byId;
        for (const WaypointMapMarker &marker : m_markers) {
            byId.insert(marker.id, marker);
        }

        QVector<QPointF> orderedPoints;
        for (const QString &id : m_routeOrder) {
            if (byId.contains(id)) {
                orderedPoints.append(toCanvas(byId.value(id).normalizedPos, area));
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
            const QPointF center = toCanvas(marker.normalizedPos, area);
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
        return;
    }

    for (const WaypointMapMarker &marker : m_markers) {
        const QPointF center = toCanvas(marker.normalizedPos, area);
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
