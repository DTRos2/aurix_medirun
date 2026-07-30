#include "mapviewwidget.h"

#include "badgelabel.h"
#include "../theme/dashboardpalette.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>

namespace {

// AURIX mascot marker geometry, ported from
// docs/dashboard_wireframe/v2/aurix_mascot_marker_states.html (symbol
// #aurix-bot, viewBox 0 0 56 56, displayed at 44x44 offset (14,18) inside a
// 72x78 card whose heading-cone apex sits at the card's (36,40) — i.e. the
// bot's own visual center). kMarkerScale converts those wireframe "card
// units" into on-canvas pixels for our much smaller map markers.
constexpr qreal kMarkerScale = 0.62;
constexpr qreal kSymbolToCard = 44.0 / 56.0;

QPointF symPoint(qreal x, qreal y)
{
    return QPointF(x * kSymbolToCard - 22.0, y * kSymbolToCard - 22.0);
}

qreal symLen(qreal v)
{
    return v * kSymbolToCard;
}

QPainterPath quadPath(const QPointF &start, const QPointF &ctrl, const QPointF &end)
{
    QPainterPath path(start);
    path.quadTo(ctrl, end);
    return path;
}

QColor colorForState(RobotMarkerState state)
{
    switch (state) {
    case RobotMarkerState::Ready:
        return DashboardPalette::RobotReady;
    case RobotMarkerState::Moving:
        return DashboardPalette::Cyan;
    case RobotMarkerState::Arrived:
        return DashboardPalette::Green;
    case RobotMarkerState::Warning:
        return DashboardPalette::Amber;
    case RobotMarkerState::ErrorStopped:
        return DashboardPalette::Red;
    }
    return DashboardPalette::RobotReady;
}

qreal coneOpacityForState(RobotMarkerState state)
{
    switch (state) {
    case RobotMarkerState::Ready:
        return 0.7;
    case RobotMarkerState::Arrived:
        return 0.8;
    case RobotMarkerState::ErrorStopped:
        return 0.6;
    default:
        return 1.0;
    }
}

// Body + antenna + ears + face window + default (open-eyed) face. Drawn
// upright regardless of heading; only the cone beneath it rotates.
void paintAurixBotBody(QPainter &painter, const QColor &bodyColor)
{
    painter.setPen(QPen(bodyColor, 2));
    painter.drawLine(symPoint(28, 11), symPoint(28, 16));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xff, 0xd4, 0x5e));
    painter.drawEllipse(symPoint(28, 8), symLen(3.4), symLen(3.4));

    painter.setPen(QPen(bodyColor, 1.4));
    painter.setBrush(DashboardPalette::CardBg);
    painter.drawEllipse(symPoint(18, 44), symLen(4.4), symLen(4.4));
    painter.drawEllipse(symPoint(38, 44), symLen(4.4), symLen(4.4));

    painter.setPen(Qt::NoPen);
    painter.setBrush(bodyColor);
    painter.drawRoundedRect(QRectF(symPoint(9, 15), QSizeF(symLen(38), symLen(27))), symLen(12), symLen(12));

    painter.setBrush(DashboardPalette::CardBg);
    painter.drawRoundedRect(QRectF(symPoint(15, 19), QSizeF(symLen(26), symLen(15))), symLen(7), symLen(7));

    painter.setBrush(Qt::white);
    painter.drawEllipse(symPoint(21.5, 26.5), symLen(3.3), symLen(3.3));
    painter.drawEllipse(symPoint(34.5, 26.5), symLen(3.3), symLen(3.3));
    painter.setBrush(QColor(0x12, 0x32, 0x44));
    painter.drawEllipse(symPoint(22, 27), symLen(1.7), symLen(1.7));
    painter.drawEllipse(symPoint(35, 27), symLen(1.7), symLen(1.7));

    painter.setBrush(Qt::white);
    painter.drawRoundedRect(QRectF(symPoint(24, 36.4), QSizeF(symLen(8), symLen(2.6))), symLen(1), symLen(1));
    painter.drawRoundedRect(QRectF(symPoint(26.7, 33.7), QSizeF(symLen(2.6), symLen(8))), symLen(1), symLen(1));
}

void paintFaceCover(QPainter &painter, qreal coverHeight)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(DashboardPalette::CardBg);
    painter.drawRoundedRect(QRectF(symPoint(16, 20), QSizeF(symLen(24), symLen(coverHeight))), symLen(5), symLen(5));
}

void paintFaceReady(QPainter &painter)
{
    paintFaceCover(painter, 11);
    QPen pen(QColor(0xa8, 0xbd, 0xd2), symLen(2.2));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(quadPath(symPoint(18, 27), symPoint(21.5, 30), symPoint(25, 27)));
    painter.drawPath(quadPath(symPoint(31, 27), symPoint(34.5, 30), symPoint(38, 27)));
}

void paintFaceArrived(QPainter &painter)
{
    paintFaceCover(painter, 12);
    QPen pen(Qt::white, symLen(2.4));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(quadPath(symPoint(18, 27), symPoint(21.5, 22.5), symPoint(25, 27)));
    painter.drawPath(quadPath(symPoint(31, 27), symPoint(34.5, 22.5), symPoint(38, 27)));
}

void paintFaceWarningExtras(QPainter &painter)
{
    QPen pen(Qt::white, symLen(1.8));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(quadPath(symPoint(18, 21.5), symPoint(21.5, 19.5), symPoint(24.5, 21)));
    painter.drawPath(quadPath(symPoint(31.5, 21), symPoint(34.5, 19.5), symPoint(38, 21.5)));

    QPainterPath drop(symPoint(47, 15));
    drop.quadTo(symPoint(51, 22), symPoint(47, 24));
    drop.quadTo(symPoint(43, 22), symPoint(47, 15));
    drop.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x8e, 0xd6, 0xff));
    painter.drawPath(drop);
}

void paintFaceErrorStopped(QPainter &painter)
{
    paintFaceCover(painter, 12);
    QPen pen(Qt::white, symLen(2.4));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(symPoint(18.5, 23), symPoint(24.5, 29));
    painter.drawLine(symPoint(24.5, 23), symPoint(18.5, 29));
    painter.drawLine(symPoint(31.5, 23), symPoint(37.5, 29));
    painter.drawLine(symPoint(37.5, 23), symPoint(31.5, 29));
}

// Field-of-view wedge, apex at the marker center, drawn in its own
// save/rotate block so only the cone (never the bot's face) turns with
// heading.
void paintHeadingCone(QPainter &painter, qreal headingDeg, const QColor &color, qreal opacity, bool withOutline)
{
    const qreal radius = 24.0;
    const qreal halfAngleDeg = 25.0;

    painter.save();
    painter.rotate(headingDeg);

    QPainterPath cone;
    cone.moveTo(0, 0);
    cone.arcTo(QRectF(-radius, -radius, radius * 2, radius * 2), 90.0 + halfAngleDeg, -2.0 * halfAngleDeg);
    cone.closeSubpath();

    QRadialGradient gradient(QPointF(0, 0), radius);
    QColor centerColor = color;
    centerColor.setAlphaF(0.55);
    QColor edgeColor = color;
    edgeColor.setAlphaF(0.0);
    gradient.setColorAt(0.0, centerColor);
    gradient.setColorAt(1.0, edgeColor);

    painter.setOpacity(opacity);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawPath(cone);

    if (withOutline) {
        QColor outlineColor = color;
        outlineColor.setAlphaF(0.45);
        painter.setPen(QPen(outlineColor, 0.8));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(cone);
    }

    painter.setOpacity(1.0);
    painter.restore();
}

void paintStateRing(QPainter &painter, RobotMarkerState state)
{
    painter.setBrush(Qt::NoBrush);
    switch (state) {
    case RobotMarkerState::Moving: {
        QColor ring = DashboardPalette::Cyan;
        ring.setAlphaF(0.32);
        painter.setPen(QPen(ring, 1.3));
        painter.drawEllipse(QPointF(0, 0), 27.0, 27.0);
        break;
    }
    case RobotMarkerState::Warning: {
        QPen pen(DashboardPalette::Amber, 1.8);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.drawEllipse(QPointF(0, 0), 26.0, 26.0);
        break;
    }
    case RobotMarkerState::ErrorStopped: {
        QColor outer = DashboardPalette::Red;
        outer.setAlphaF(0.5);
        painter.setPen(QPen(outer, 1.2));
        painter.drawEllipse(QPointF(0, 0), 30.0, 30.0);
        painter.setPen(QPen(DashboardPalette::Red, 2.2));
        painter.drawEllipse(QPointF(0, 0), 26.5, 26.5);
        break;
    }
    default:
        break;
    }
}

void paintRobotMarker(QPainter &painter, const QPointF &center, RobotMarkerState state, qreal headingDeg)
{
    const QColor color = colorForState(state);

    painter.save();
    painter.translate(center);
    painter.scale(kMarkerScale, kMarkerScale);

    paintStateRing(painter, state);
    paintHeadingCone(painter, headingDeg, color, coneOpacityForState(state), state == RobotMarkerState::Moving);
    paintAurixBotBody(painter, color);

    switch (state) {
    case RobotMarkerState::Ready:
        paintFaceReady(painter);
        break;
    case RobotMarkerState::Arrived:
        paintFaceArrived(painter);
        break;
    case RobotMarkerState::Warning:
        paintFaceWarningExtras(painter);
        break;
    case RobotMarkerState::ErrorStopped:
        paintFaceErrorStopped(painter);
        break;
    case RobotMarkerState::Moving:
        break;
    }

    painter.restore();
}

void paintWaypointMarker(QPainter &painter, const QPointF &center)
{
    painter.setPen(QPen(DashboardPalette::TextMuted, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 7, 7);
}

void paintDestinationMarker(QPainter &painter, const QPointF &center)
{
    painter.setPen(QPen(DashboardPalette::TextPrimary, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 9, 9);
    painter.drawLine(QPointF(center.x(), center.y() - 13), QPointF(center.x(), center.y() - 6));
    painter.drawLine(QPointF(center.x(), center.y() + 6), QPointF(center.x(), center.y() + 13));
    painter.drawLine(QPointF(center.x() - 13, center.y()), QPointF(center.x() - 6, center.y()));
    painter.drawLine(QPointF(center.x() + 13, center.y()), QPointF(center.x() + 6, center.y()));
    painter.setPen(Qt::NoPen);
    painter.setBrush(DashboardPalette::TextPrimary);
    painter.drawEllipse(center, 3, 3);
}

void paintHomeMarker(QPainter &painter, const QPointF &center)
{
    painter.setPen(QPen(DashboardPalette::TextMuted, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 7, 7);

    QFont font = painter.font();
    font.setPixelSize(9);
    painter.setFont(font);
    painter.setPen(DashboardPalette::TextTertiary);
    painter.drawText(QRectF(center.x() - 22, center.y() + 8, 44, 12), Qt::AlignCenter, QStringLiteral("HOME"));
}

} // namespace

MapViewWidget::MapViewWidget(QWidget *parent)
    : QFrame(parent)
    , m_mode(MapViewMode::Live)
    , m_robotState(RobotMarkerState::Ready)
    , m_robotPos(0.2, 0.8)
    , m_robotHeadingDeg(0.0)
    , m_dashedPathColor(DashboardPalette::Cyan)
    , m_hasDestination(false)
    , m_hasHome(false)
    , m_recording(false)
{
    setObjectName(QStringLiteral("mapViewCanvas"));
    setMinimumSize(160, 90);

    m_captionLabel = new QLabel(this);
    m_captionLabel->setObjectName(QStringLiteral("mapCaption"));
    m_captionLabel->hide();

    m_recBadge = makeBadgeLabel(QStringLiteral("● REC"), QStringLiteral("red"), this);
    m_recBadge->hide();

    m_followChip = new QPushButton(QStringLiteral("로봇 따라가기"), this);
    m_followChip->setObjectName(QStringLiteral("mapToggleChip"));
    m_followChip->setCheckable(true);
    m_followChip->setChecked(true);
    m_followChip->hide();

    m_hidePathChip = new QPushButton(QStringLiteral("경로 숨기기"), this);
    m_hidePathChip->setObjectName(QStringLiteral("mapToggleChip"));
    m_hidePathChip->setCheckable(true);
    m_hidePathChip->hide();
}

void MapViewWidget::setMode(MapViewMode mode)
{
    m_mode = mode;
    const bool live = (mode == MapViewMode::Live);
    m_followChip->setVisible(live);
    m_hidePathChip->setVisible(live);
    m_recBadge->setVisible(live && m_recording);
    layoutOverlay();
    update();
}

void MapViewWidget::setCaption(const QString &text)
{
    m_captionLabel->setText(text);
    m_captionLabel->setVisible(!text.isEmpty());
    layoutOverlay();
}

void MapViewWidget::setRecording(bool recording)
{
    m_recording = recording;
    m_recBadge->setVisible(recording && m_mode == MapViewMode::Live);
    layoutOverlay();
}

void MapViewWidget::setRobotPose(const QPointF &normalizedPos, qreal headingDegrees)
{
    m_robotPos = normalizedPos;
    m_robotHeadingDeg = headingDegrees;
    update();
}

void MapViewWidget::setRobotState(RobotMarkerState state)
{
    m_robotState = state;
    update();
}

void MapViewWidget::setPlannedPath(const QVector<QPointF> &normalizedPoints)
{
    m_solidPath.clear();
    m_dashedPath = normalizedPoints;
    m_dashedPathColor = DashboardPalette::Cyan;
    update();
}

void MapViewWidget::setPlaybackPath(const QVector<QPointF> &traveled, const QVector<QPointF> &remaining)
{
    m_solidPath = traveled;
    m_dashedPath = remaining;
    m_dashedPathColor = DashboardPalette::RobotReady;
    update();
}

void MapViewWidget::setWaypoints(const QVector<QPointF> &normalizedPoints)
{
    m_waypoints = normalizedPoints;
    update();
}

void MapViewWidget::setDestination(const QPointF &normalizedPos, bool visible)
{
    m_destination = normalizedPos;
    m_hasDestination = visible;
    update();
}

void MapViewWidget::setHome(const QPointF &normalizedPos, bool visible)
{
    m_home = normalizedPos;
    m_hasHome = visible;
    update();
}

QRectF MapViewWidget::drawableArea() const
{
    const qreal margin = 10.0;
    return QRectF(margin, margin, width() - margin * 2, height() - margin * 2);
}

QPointF MapViewWidget::toCanvas(const QPointF &normalized, const QRectF &area) const
{
    return QPointF(area.left() + normalized.x() * area.width(),
                    area.top() + normalized.y() * area.height());
}

void MapViewWidget::layoutOverlay()
{
    m_captionLabel->adjustSize();
    m_captionLabel->move(9, 7);

    m_recBadge->adjustSize();
    m_recBadge->move(width() - m_recBadge->width() - 9, 7);

    m_followChip->adjustSize();
    m_hidePathChip->adjustSize();
    const int chipY = height() - m_followChip->height() - 7;
    m_followChip->move(9, chipY);
    m_hidePathChip->move(9 + m_followChip->width() + 5, chipY);
}

void MapViewWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    layoutOverlay();
}

void MapViewWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF area = drawableArea();

    painter.setPen(QPen(DashboardPalette::BorderGuide, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(area);

    if (m_mode != MapViewMode::Reference) {
        if (m_solidPath.size() >= 2) {
            QPainterPath path;
            path.moveTo(toCanvas(m_solidPath.first(), area));
            for (int i = 1; i < m_solidPath.size(); ++i) {
                path.lineTo(toCanvas(m_solidPath.at(i), area));
            }
            painter.setPen(QPen(DashboardPalette::Cyan, 2.5));
            painter.drawPath(path);
        }

        if (m_dashedPath.size() >= 2) {
            QPainterPath path;
            path.moveTo(toCanvas(m_dashedPath.first(), area));
            for (int i = 1; i < m_dashedPath.size(); ++i) {
                path.lineTo(toCanvas(m_dashedPath.at(i), area));
            }
            painter.setPen(QPen(m_dashedPathColor, 2.5, Qt::DashLine));
            painter.drawPath(path);
        }

        for (const QPointF &waypoint : m_waypoints) {
            paintWaypointMarker(painter, toCanvas(waypoint, area));
        }

        if (m_hasDestination) {
            paintDestinationMarker(painter, toCanvas(m_destination, area));
        }
    }

    if (m_hasHome) {
        paintHomeMarker(painter, toCanvas(m_home, area));
    }

    paintRobotMarker(painter, toCanvas(m_robotPos, area), m_robotState, m_robotHeadingDeg);
}
