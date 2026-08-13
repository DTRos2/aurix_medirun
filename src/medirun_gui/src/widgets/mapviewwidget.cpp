#include "mapviewwidget.h"

#include "../theme/dashboardpalette.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kClickVsDragPx = 5.0;

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

void paintInnerRadarSweep(QPainter &painter, qreal headingDeg, const QColor &color)
{
    painter.save();
    painter.rotate(headingDeg);

    QColor sweepColor = color;
    sweepColor.setAlphaF(0.55);
    QPen pen(sweepColor, symLen(1.4));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QPointF center = symPoint(28, 26.5);
    painter.drawLine(center, center + QPointF(0.0, -symLen(7.0)));
    painter.drawArc(QRectF(symPoint(22, 20.5), QSizeF(symLen(12), symLen(12))), 50 * 16, 80 * 16);
    painter.drawArc(QRectF(symPoint(19.5, 18.0), QSizeF(symLen(17), symLen(17))), 58 * 16, 64 * 16);

    painter.restore();
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
    paintInnerRadarSweep(painter, headingDeg, color);

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

// 웨이포인트가 도착 시 로봇이 바라보게 될 방향(yaw, map 프레임)을 짧은
// 화살표로 표시 — WaypointMapWidget::paintYawIndicator와 동일 규칙(이미지
// 행(row) 축은 world y축과 반대 방향이라 dy 부호를 뒤집는다).
void paintYawIndicator(QPainter &painter, const QPointF &center, double yawRadians, const QColor &color)
{
    constexpr double kArrowLengthPx = 12.0;
    const double dx = std::cos(yawRadians) * kArrowLengthPx;
    const double dy = -std::sin(yawRadians) * kArrowLengthPx;
    const double len = std::hypot(dx, dy);
    if (len < 1e-6) {
        return;
    }
    const QPointF tip(center.x() + dx, center.y() + dy);
    painter.setPen(QPen(color, 1.6));
    painter.drawLine(center, tip);

    const double ux = dx / len;
    const double uy = dy / len;
    constexpr double kHeadLen = 4.0;
    constexpr double kSpread = 0.55;
    auto rotate = [](double x, double y, double angle) {
        return QPointF(x * std::cos(angle) - y * std::sin(angle), x * std::sin(angle) + y * std::cos(angle));
    };
    painter.drawLine(tip, tip + rotate(-ux, -uy, kSpread) * kHeadLen);
    painter.drawLine(tip, tip + rotate(-ux, -uy, -kSpread) * kHeadLen);
}

void paintWaypointMarker(QPainter &painter, const QPointF &center, double yawRadians)
{
    painter.setPen(QPen(DashboardPalette::TextMuted, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 7, 7);
    paintYawIndicator(painter, center, yawRadians, DashboardPalette::TextMuted);
}

// 웨이포인트 관리 페이지의 목표 마커(Amber 이중 링 + 크로스헤어)와 같은
// 스타일 — 다만 여기서는 로봇 위치까지의 점선 안내선은 그리지 않는다.
void paintDestinationMarker(QPainter &painter, const QPointF &center)
{
    painter.setPen(QPen(DashboardPalette::Amber, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 20, 20);
    painter.setPen(QPen(DashboardPalette::Amber, 1.3, Qt::DashLine));
    painter.drawEllipse(center, 14, 14);
    painter.setPen(QPen(DashboardPalette::Amber, 1.5));
    painter.drawLine(QPointF(center.x() - 24, center.y()), QPointF(center.x() - 10, center.y()));
    painter.drawLine(QPointF(center.x() + 10, center.y()), QPointF(center.x() + 24, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - 24), QPointF(center.x(), center.y() - 10));
    painter.drawLine(QPointF(center.x(), center.y() + 10), QPointF(center.x(), center.y() + 24));
}

void paintHomeMarker(QPainter &painter, const QPointF &center)
{
    // WaypointManagementPage의 Home 글리프(원 + 지붕선)를 재사용하되, 원격
    // 제어 지도는 배경 이미지 위라 대비가 약해 눈에 띄도록 Green으로 키웠다.
    painter.setPen(QPen(DashboardPalette::Green, 2.2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 9, 9);
    painter.drawLine(QPointF(center.x() - 5, center.y() + 2), QPointF(center.x(), center.y() - 4));
    painter.drawLine(QPointF(center.x(), center.y() - 4), QPointF(center.x() + 5, center.y() + 2));

    QFont font = painter.font();
    font.setPixelSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(DashboardPalette::Green);
    painter.drawText(QRectF(center.x() - 24, center.y() + 10, 48, 13), Qt::AlignCenter, QStringLiteral("HOME"));
}

void paintCallZoneMarker(QPainter &painter, const QPointF &center, const QString &label)
{
    // WaypointManagementPage의 CallZone 글리프(채워진 원)를 재사용하되, Amber로
    // 색을 바꿔 Home(Green)과 한눈에 구분되고 배경 위에서도 잘 보이게 했다.
    painter.setPen(QPen(DashboardPalette::Amber, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, 10, 10);
    painter.setPen(Qt::NoPen);
    painter.setBrush(DashboardPalette::Amber);
    painter.drawEllipse(center, 3.5, 3.5);

    if (!label.isEmpty()) {
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(DashboardPalette::Amber);
        painter.drawText(QRectF(center.x() - 30, center.y() + 11, 60, 13), Qt::AlignCenter, label);
    }
}

} // namespace

MapViewWidget::MapViewWidget(QWidget *parent)
    : QFrame(parent)
    , m_poseSettingEnabled(false)
    , m_dragging(false)
    , m_mode(MapViewMode::Live)
    , m_robotState(RobotMarkerState::Ready)
    , m_robotPos(0.2, 0.8)
    , m_robotHeadingDeg(0.0)
    , m_dashedPathColor(DashboardPalette::Cyan)
    , m_hasDestination(false)
    , m_hasHome(false)
    , m_robotVisible(true)
    , m_overlayChipsVisible(true)
{
    setObjectName(QStringLiteral("mapViewCanvas"));
    setMinimumSize(160, 90);

    m_captionLabel = new QLabel(this);
    m_captionLabel->setObjectName(QStringLiteral("mapCaption"));
    m_captionLabel->hide();

    m_followChip = new QPushButton(QStringLiteral("로봇 따라가기"), this);
    m_followChip->setObjectName(QStringLiteral("mapToggleChip"));
    m_followChip->setCheckable(true);
    m_followChip->setChecked(false);
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
    m_followChip->setVisible(live && m_overlayChipsVisible);
    m_hidePathChip->setVisible(live && m_overlayChipsVisible);
    layoutOverlay();
    update();
}

void MapViewWidget::setCaption(const QString &text)
{
    if (m_captionLabel->text() == text) {
        return;
    }
    m_captionLabel->setText(text);
    m_captionLabel->setVisible(!text.isEmpty());
    layoutOverlay();
}


void MapViewWidget::setPoseSettingEnabled(bool enabled)
{
    m_poseSettingEnabled = enabled;
    m_dragging = false;
    update();
}

void MapViewWidget::setOverlayChipsVisible(bool visible)
{
    m_overlayChipsVisible = visible;
    const bool live = (m_mode == MapViewMode::Live);
    m_followChip->setVisible(live && m_overlayChipsVisible);
    m_hidePathChip->setVisible(live && m_overlayChipsVisible);
    layoutOverlay();
}

void MapViewWidget::setBackgroundImage(const QImage &image)
{
    if (image.isNull()) {
        clearBackgroundImage();
        return;
    }

    m_originalBackgroundImage = image;
    m_backgroundPixmap = QPixmap();
    m_backgroundPixmapTargetSize = QSize();
    updateScaledBackgroundPixmap();
    update();
}

void MapViewWidget::clearBackgroundImage()
{
    m_originalBackgroundImage = QImage();
    m_backgroundPixmap = QPixmap();
    m_backgroundPixmapTargetSize = QSize();
    update();
}
void MapViewWidget::setRobotPose(const QPointF &normalizedPos, qreal headingDegrees)
{
    if (qFuzzyCompare(m_robotPos.x(), normalizedPos.x())
        && qFuzzyCompare(m_robotPos.y(), normalizedPos.y())
        && qFuzzyCompare(m_robotHeadingDeg, headingDegrees)) {
        return;
    }
    m_robotPos = normalizedPos;
    m_robotHeadingDeg = headingDegrees;
    update();
}

void MapViewWidget::setRobotVisible(bool visible)
{
    if (m_robotVisible == visible) {
        return;
    }
    m_robotVisible = visible;
    update();
}

void MapViewWidget::setRobotState(RobotMarkerState state)
{
    if (m_robotState == state) {
        return;
    }
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

void MapViewWidget::setWaypoints(const QVector<QPointF> &normalizedPoints, const QVector<double> &yawRadians)
{
    m_waypoints = normalizedPoints;
    m_waypointYaws = yawRadians;
    update();
}

void MapViewWidget::setCallZones(const QVector<QPointF> &normalizedPoints, const QStringList &labels)
{
    m_callZones = normalizedPoints;
    m_callZoneLabels = labels;
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

QRectF MapViewWidget::imageRect() const
{
    const QRectF area = drawableArea();

    if (m_originalBackgroundImage.isNull()) {
        return area;
    }

    const QSize scaledSize = m_originalBackgroundImage.size().scaled(
        area.size().toSize(), Qt::KeepAspectRatio);
    const QPointF topLeft(
        area.left() + (area.width() - scaledSize.width()) / 2.0,
        area.top() + (area.height() - scaledSize.height()) / 2.0);
    return QRectF(topLeft, QSizeF(scaledSize));
}

QPointF MapViewWidget::toCanvas(const QPointF &normalized, const QRectF &area) const
{
    return QPointF(area.left() + normalized.x() * area.width(),
                    area.top() + normalized.y() * area.height());
}

QPointF MapViewWidget::canvasToNormalized(const QPointF &canvasPos, const QRectF &area) const
{
    if (area.width() <= 0.0 || area.height() <= 0.0) {
        return QPointF(0.0, 0.0);
    }
    const qreal u = (canvasPos.x() - area.left()) / area.width();
    const qreal v = (canvasPos.y() - area.top()) / area.height();
    return QPointF(std::clamp(u, 0.0, 1.0), std::clamp(v, 0.0, 1.0));
}

void MapViewWidget::layoutOverlay()
{
    m_captionLabel->adjustSize();
    m_captionLabel->move(9, 7);

    m_followChip->adjustSize();
    m_hidePathChip->adjustSize();
    const int chipY = height() - m_followChip->height() - 7;
    m_followChip->move(9, chipY);
    m_hidePathChip->move(9 + m_followChip->width() + 5, chipY);
}

void MapViewWidget::updateScaledBackgroundPixmap()
{
    if (m_originalBackgroundImage.isNull()) {
        m_backgroundPixmap = QPixmap();
        m_backgroundPixmapTargetSize = QSize();
        return;
    }

    const QSize targetSize = imageRect().size().toSize();
    if (!targetSize.isValid()) {
        m_backgroundPixmap = QPixmap();
        m_backgroundPixmapTargetSize = QSize();
        return;
    }
    if (!m_backgroundPixmap.isNull() && m_backgroundPixmapTargetSize == targetSize) {
        return;
    }

    const qreal dpr = devicePixelRatioF();
    const QSize scaledTarget(
        std::max(1, qRound(targetSize.width() * dpr)),
        std::max(1, qRound(targetSize.height() * dpr)));
    const QImage scaledImage = m_originalBackgroundImage.scaled(
        scaledTarget,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    m_backgroundPixmap = QPixmap::fromImage(scaledImage);
    m_backgroundPixmap.setDevicePixelRatio(dpr);
    m_backgroundPixmapTargetSize = targetSize;
}

void MapViewWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    layoutOverlay();
    updateScaledBackgroundPixmap();
}

void MapViewWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_poseSettingEnabled || event->button() != Qt::LeftButton) {
        QFrame::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_dragStartPos = event->position();
    m_dragCurrentPos = m_dragStartPos;
    update();
}

void MapViewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    m_dragCurrentPos = event->position();
    update();
}

void MapViewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QFrame::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    const qreal dxPx = m_dragCurrentPos.x() - m_dragStartPos.x();
    const qreal dyPx = m_dragCurrentPos.y() - m_dragStartPos.y();
    const qreal dragDistance = std::hypot(dxPx, dyPx);

    // 이미지 행(row) 축은 world y축과 반대 방향 — WaypointMapWidget과 동일 규칙.
    const double yaw = dragDistance < kClickVsDragPx ? 0.0 : std::atan2(-dyPx, dxPx);
    const QPointF normalized = canvasToNormalized(m_dragStartPos, imageRect());
    emit posePickRequested(normalized, yaw);

    update();
}

void MapViewWidget::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF area = drawableArea();
    const QRectF imgRect = imageRect();
    updateScaledBackgroundPixmap();

    if (!m_backgroundPixmap.isNull()) {
        painter.drawPixmap(imgRect.topLeft(), m_backgroundPixmap);
    }

    painter.setPen(QPen(DashboardPalette::BorderGuide, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(area);

    if (m_dragging) {
        painter.setPen(QPen(DashboardPalette::Cyan, 1, Qt::DashLine));
        painter.drawLine(m_dragStartPos, m_dragCurrentPos);
        painter.setPen(Qt::NoPen);
        painter.setBrush(DashboardPalette::Cyan);
        painter.drawEllipse(m_dragStartPos, 3, 3);
    }

    if (m_mode != MapViewMode::Reference) {
        if (m_solidPath.size() >= 2) {
            QPainterPath path;
            path.moveTo(toCanvas(m_solidPath.first(), imgRect));
            for (int i = 1; i < m_solidPath.size(); ++i) {
                path.lineTo(toCanvas(m_solidPath.at(i), imgRect));
            }
            painter.setPen(QPen(DashboardPalette::Cyan, 2.5));
            painter.drawPath(path);
        }

        if (m_dashedPath.size() >= 2) {
            QPainterPath path;
            path.moveTo(toCanvas(m_dashedPath.first(), imgRect));
            for (int i = 1; i < m_dashedPath.size(); ++i) {
                path.lineTo(toCanvas(m_dashedPath.at(i), imgRect));
            }
            painter.setPen(QPen(m_dashedPathColor, 2.5, Qt::DashLine));
            painter.drawPath(path);
        }

        if (m_hasDestination) {
            paintDestinationMarker(painter, toCanvas(m_destination, imgRect));
        }
    }

    // 웨이포인트/콜존/홈 마커는 Reference 모드(RemoteControlPage 미니맵)에서도
    // 상시 표시 — 주행 경로/목적지만 Live/Playback 전용.
    for (int i = 0; i < m_waypoints.size(); ++i) {
        const double yaw = i < m_waypointYaws.size() ? m_waypointYaws.at(i) : 0.0;
        paintWaypointMarker(painter, toCanvas(m_waypoints.at(i), imgRect), yaw);
    }

    for (int i = 0; i < m_callZones.size(); ++i) {
        const QString label = i < m_callZoneLabels.size() ? m_callZoneLabels.at(i) : QString();
        paintCallZoneMarker(painter, toCanvas(m_callZones.at(i), imgRect), label);
    }

    if (m_hasHome) {
        paintHomeMarker(painter, toCanvas(m_home, imgRect));
    }

    if (m_robotVisible) {
        paintRobotMarker(painter, toCanvas(m_robotPos, imgRect), m_robotState, m_robotHeadingDeg);
    }
}