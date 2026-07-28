#include "remotecontrolpage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/mapviewwidget.h"
#include "../widgets/proportionalbar.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QLabel *makeLabel(const QString &text, const QString &objectName = {})
{
    auto *label = new QLabel(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    label->setWordWrap(true);
    return label;
}

QFrame *panelFrame(const QString &objectName)
{
    auto *frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

QPushButton *directionButton(const QString &text, bool danger = false)
{
    auto *button = new QPushButton(text);
    button->setObjectName(danger ? QStringLiteral("dangerFilledButton") : QStringLiteral("directionPadButton"));
    button->setFixedSize(38, 38);
    return button;
}
} // namespace

RemoteControlPage::RemoteControlPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildManualControlBanner());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);
    bodyRow->addWidget(buildCameraPanel(), 15);

    auto *rightCol = new QVBoxLayout;
    rightCol->setSpacing(9);
    rightCol->addWidget(buildMinimapPanel());
    rightCol->addWidget(buildDirectionPad());
    rightCol->addWidget(buildSpeedLimitPanel());

    auto *dockButton = new QPushButton(QStringLiteral("도킹 재시도"));
    auto *autonomyButton = new QPushButton(QStringLiteral("자율 모드 복귀"));
    rightCol->addWidget(dockButton);
    rightCol->addWidget(autonomyButton);
    rightCol->addStretch();

    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(196);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    layout->addLayout(bodyRow, 1);
}

QWidget *RemoteControlPage::buildManualControlBanner()
{
    auto *panel = panelFrame(QStringLiteral("anomalyCard"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(8);

    auto *title = makeLabel(QStringLiteral("🎮  수동 제어 활성"));
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Red.name()));
    layout->addWidget(title);
    layout->addWidget(makeLabel(QStringLiteral("자율 목표 취소됨 · 제어권 qt / admin_lee"), QStringLiteral("metricCaption")));
    layout->addStretch();
    return panel;
}

QWidget *RemoteControlPage::buildCameraPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    panel->setMinimumHeight(230);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(11, 9, 11, 9);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(makeBadgeLabel(QStringLiteral("ArUco 미검출"), QStringLiteral("amber"), panel));
    topRow->addStretch();
    topRow->addWidget(makeLabel(QStringLiteral("지연 120ms"), QStringLiteral("metricCaption")));
    layout->addLayout(topRow);

    layout->addStretch();
    auto *centerLabel = makeLabel(QStringLiteral("🎥  카메라 라이브 · 14FPS"));
    centerLabel->setAlignment(Qt::AlignCenter);
    centerLabel->setStyleSheet(QStringLiteral("color: %1;").arg(DashboardPalette::TextTertiary.name()));
    layout->addWidget(centerLabel);
    layout->addStretch();

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addStretch();
    auto *front = makeLabel(QStringLiteral("전방 0.42m"));
    front->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; padding: 3px 9px; border: 1px solid #2a4a66; border-radius: 6px;").arg(DashboardPalette::Green.name()));
    bottomRow->addWidget(front);
    auto *left = makeLabel(QStringLiteral("좌 0.88m"));
    left->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; padding: 3px 9px; border: 1px solid #2a4a66; border-radius: 6px;").arg(DashboardPalette::TextSecondary.name()));
    bottomRow->addWidget(left);
    auto *right = makeLabel(QStringLiteral("우 1.14m"));
    right->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; padding: 3px 9px; border: 1px solid #2a4a66; border-radius: 6px;").arg(DashboardPalette::TextSecondary.name()));
    bottomRow->addWidget(right);
    bottomRow->addStretch();
    layout->addLayout(bottomRow);
    return panel;
}

QWidget *RemoteControlPage::buildMinimapPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *mapView = new MapViewWidget(panel);
    mapView->setFixedSize(184, 100);
    mapView->setMode(MapViewMode::Reference);
    mapView->setCaption(QStringLiteral("미니맵 · 방향 참조용"));
    mapView->setRobotPose(QPointF(0.35, 0.65), 0.0);
    mapView->setRobotState(RobotMarkerState::Warning);
    mapView->setHome(QPointF(0.78, 0.3));
    layout->addWidget(mapView);
    return panel;
}

QWidget *RemoteControlPage::buildDirectionPad()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QGridLayout(panel);
    layout->setContentsMargins(9, 10, 9, 10);
    layout->setSpacing(5);

    layout->addWidget(directionButton(QStringLiteral("▲")), 0, 1);
    layout->addWidget(directionButton(QStringLiteral("↺")), 1, 0);
    layout->addWidget(directionButton(QStringLiteral("정지"), true), 1, 1);
    layout->addWidget(directionButton(QStringLiteral("↻")), 1, 2);
    layout->addWidget(directionButton(QStringLiteral("▼")), 2, 1);
    return panel;
}

QWidget *RemoteControlPage::buildSpeedLimitPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(makeLabel(QStringLiteral("속도 제한"), QStringLiteral("metricCaption")));
    topRow->addStretch();
    auto *valueLabel = makeLabel(QStringLiteral("0.12 m/s"));
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Cyan.name()));
    topRow->addWidget(valueLabel);
    layout->addLayout(topRow);
    layout->addWidget(makeProportionalBar(40, DashboardPalette::Cyan, panel));
    return panel;
}
