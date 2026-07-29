#include "slammappingpage.h"

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

QHBoxLayout *statRow(QBoxLayout *parentLayout, const QString &label, const QString &value, const QColor &valueColor = DashboardPalette::TextPrimary)
{
    auto *row = new QHBoxLayout;
    row->addWidget(makeLabel(label, QStringLiteral("metricCaption")));
    row->addStretch();
    auto *valueLabel = makeLabel(value);
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(valueColor.name()));
    row->addWidget(valueLabel);
    parentLayout->addLayout(row);
    return row;
}

QPushButton *directionButton(const QString &text, bool danger = false)
{
    auto *button = new QPushButton(text);
    button->setObjectName(danger ? QStringLiteral("dangerFilledButton") : QStringLiteral("directionPadButton"));
    button->setFixedSize(38, 38);
    return button;
}
} // namespace

SlamMappingPage::SlamMappingPage(QWidget *parent)
    : QWidget(parent)
    , m_mapView(nullptr)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildStatusBanner());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);

    m_mapView = new MapViewWidget(canvas);
    m_mapView->setMinimumHeight(250);
    m_mapView->setMode(MapViewMode::Live);
    m_mapView->setCaption(QStringLiteral("B1 승강장 · 탐색 진행 중"));
    m_mapView->setRecording(true);
    m_mapView->setRobotPose(QPointF(0.34, 0.58), 40.0);
    m_mapView->setRobotState(RobotMarkerState::Moving);
    m_mapView->setPlannedPath({QPointF(0.2, 0.82), QPointF(0.34, 0.82), QPointF(0.34, 0.58)});
    leftCol->addWidget(m_mapView, 1);
    leftCol->addWidget(buildActionRow());
    bodyRow->addLayout(leftCol, 7);

    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);
    rightCol->addWidget(buildMappingStatusPanel());
    rightCol->addWidget(buildDirectionPad());
    rightCol->addWidget(buildSpeedLimitPanel(), 1);

    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(200);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    layout->addLayout(bodyRow, 1);
}

QWidget *SlamMappingPage::buildStatusBanner()
{
    auto *panel = panelFrame(QStringLiteral("anomalyCard"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(8);

    auto *title = makeLabel(QStringLiteral("🗺  매핑 진행 중"));
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Cyan.name()));
    layout->addWidget(title);
    layout->addWidget(makeLabel(QStringLiteral("경과 03:12 · 스캔 매칭 정상 · 루프 클로저 1회"), QStringLiteral("metricCaption")));
    layout->addStretch();
    layout->addWidget(makeBadgeLabel(QStringLiteral("SLAM 활성"), QStringLiteral("cyan"), panel));
    return panel;
}

QWidget *SlamMappingPage::buildActionRow()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    auto *startButton = new QPushButton(QStringLiteral("매핑 시작"));
    auto *stopButton = new QPushButton(QStringLiteral("매핑 정지"));
    stopButton->setObjectName(QStringLiteral("dangerFilledButton"));
    auto *saveButton = new QPushButton(QStringLiteral("지도 저장"));
    saveButton->setObjectName(QStringLiteral("primaryCtaButton"));
    auto *resetButton = new QPushButton(QStringLiteral("초기화"));

    for (QPushButton *button : {startButton, stopButton, saveButton, resetButton}) {
        layout->addWidget(button);
    }
    return row;
}

QWidget *SlamMappingPage::buildMappingStatusPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("매핑 상태"), QStringLiteral("metricCaption")));

    statRow(layout, QStringLiteral("지도 커버리지"), QStringLiteral("62%"));
    layout->addWidget(makeProportionalBar(62, DashboardPalette::Cyan, panel));
    layout->addSpacing(4);
    statRow(layout, QStringLiteral("스캔 매칭"), QStringLiteral("정상"), DashboardPalette::Green);
    statRow(layout, QStringLiteral("루프 클로저"), QStringLiteral("1회"));
    statRow(layout, QStringLiteral("저장 형식"), QStringLiteral("pgm/yaml"));
    return panel;
}

QWidget *SlamMappingPage::buildDirectionPad()
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

QWidget *SlamMappingPage::buildSpeedLimitPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(makeLabel(QStringLiteral("속도 제한"), QStringLiteral("metricCaption")));
    topRow->addStretch();
    auto *valueLabel = makeLabel(QStringLiteral("0.15 m/s"));
    valueLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Cyan.name()));
    topRow->addWidget(valueLabel);
    layout->addLayout(topRow);
    layout->addWidget(makeProportionalBar(50, DashboardPalette::Cyan, panel));
    layout->addStretch();
    return panel;
}
