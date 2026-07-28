#include "mapviewwidget.h"

#include "badgelabel.h"
#include "../theme/dashboardpalette.h"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>

namespace {

QPolygonF headingTriangle()
{
    return QPolygonF({QPointF(0, -6), QPointF(5, 6), QPointF(-5, 6)});
}

void paintRobotMarker(QPainter &painter, const QPointF &center, RobotMarkerState state, qreal headingDeg)
{
    QColor fill;
    QColor ringColor;
    qreal ringWidth = 0;
    Qt::PenStyle ringStyle = Qt::SolidLine;
    bool doubleRing = false;

    switch (state) {
    case RobotMarkerState::Ready:
        fill = DashboardPalette::RobotReady;
        break;
    case RobotMarkerState::Moving:
        fill = DashboardPalette::Cyan;
        ringColor = DashboardPalette::Cyan;
        ringColor.setAlpha(90);
        ringWidth = 5;
        break;
    case RobotMarkerState::Arrived:
        fill = DashboardPalette::Green;
        break;
    case RobotMarkerState::Warning:
        fill = DashboardPalette::Amber;
        ringColor = DashboardPalette::Amber;
        ringWidth = 6;
        ringStyle = Qt::DashLine;
        break;
    case RobotMarkerState::ErrorStopped:
        fill = DashboardPalette::Red;
        ringColor = DashboardPalette::Red;
        ringWidth = 9;
        doubleRing = true;
        break;
    }

    const qreal robotRadius = 9.0;

    if (ringWidth > 0) {
        QColor outerRing = ringColor;
        outerRing.setAlpha(state == RobotMarkerState::ErrorStopped ? 90 : outerRing.alpha());
        painter.setPen(QPen(outerRing, 1.5, ringStyle));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, robotRadius + ringWidth, robotRadius + ringWidth);
        if (doubleRing) {
            QColor innerRing = ringColor;
            innerRing.setAlpha(220);
            painter.setPen(QPen(innerRing, 2.2));
            painter.drawEllipse(center, robotRadius + ringWidth * 0.55, robotRadius + ringWidth * 0.55);
        }
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawEllipse(center, robotRadius, robotRadius);

    painter.save();
    painter.translate(center);
    painter.rotate(headingDeg);
    painter.setBrush(DashboardPalette::CardBg);
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(headingTriangle());
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
