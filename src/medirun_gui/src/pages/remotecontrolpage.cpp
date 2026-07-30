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
#include <QSizePolicy>
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

    auto *bigSlotWidget = new QWidget(canvas);
    bigSlotWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_bigSlot = new QVBoxLayout(bigSlotWidget);
    m_bigSlot->setContentsMargins(0, 0, 0, 0);
    bodyRow->addWidget(bigSlotWidget, 15);

    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);

    auto *smallSlotWidget = new QWidget;
    smallSlotWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_smallSlot = new QVBoxLayout(smallSlotWidget);
    m_smallSlot->setContentsMargins(0, 0, 0, 0);
    rightCol->addWidget(smallSlotWidget);

    auto *swapButton = buildViewSwapButton();
    swapButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(swapButton);

    m_cameraPanel = buildCameraPanel();
    m_minimapPanel = buildMinimapPanel();
    m_bigSlot->addWidget(m_cameraPanel);
    m_smallSlot->addWidget(m_minimapPanel);

    auto *directionPad = buildDirectionPad();
    directionPad->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(directionPad);

    auto *speedPresetPanel = buildSpeedPresetPanel();
    speedPresetPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(speedPresetPanel);

    auto *cmdVelPanel = buildCmdVelPanel();
    cmdVelPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    rightCol->addWidget(cmdVelPanel);

    rightCol->addStretch(1);

    auto *bottomButtonRow = new QHBoxLayout;
    bottomButtonRow->setSpacing(6);
    auto *dockButton = new QPushButton(QStringLiteral("도킹 재시도"));
    dockButton->setObjectName(QStringLiteral("cyanOutlineButton"));
    dockButton->setMinimumHeight(46);
    auto *autonomyButton = new QPushButton(QStringLiteral("자율 복귀"));
    autonomyButton->setMinimumHeight(46);
    bottomButtonRow->addWidget(dockButton, 1);
    bottomButtonRow->addWidget(autonomyButton, 1);
    rightCol->addLayout(bottomButtonRow);

    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(224);
    rightColWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Red.name()));
    layout->addWidget(title);
    auto *caption = makeLabel(QStringLiteral("자율 목표 취소됨 · 제어권 qt / admin_lee"), QStringLiteral("metricCaption"));
    caption->setWordWrap(false);
    layout->addWidget(caption);
    layout->addStretch();
    return panel;
}

QWidget *RemoteControlPage::buildCameraPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    panel->setMinimumHeight(230);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
    // MapViewWidget already paints its own dashed extent border, so the
    // panel's own QSS border is dropped here to avoid a doubled box outline.
    panel->setStyleSheet(QStringLiteral("background-color: %1; border: none; border-radius: 8px;").arg(DashboardPalette::PanelBg.name()));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);

    m_mapView = new MapViewWidget(panel);
    // Fixed height only; width follows the panel so the dashed extent rect
    // (drawn relative to the widget's own size) hugs both left and right
    // edges symmetrically instead of leaving dead space on the right.
    m_mapView->setFixedHeight(100);
    m_mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_mapView->setMode(MapViewMode::Reference);
    m_mapView->setCaption(QStringLiteral("미니맵 · 방향 참조용"));
    m_mapView->setRobotPose(QPointF(0.35, 0.65), 0.0);
    m_mapView->setRobotState(RobotMarkerState::Warning);
    m_mapView->setHome(QPointF(0.78, 0.3));
    layout->addWidget(m_mapView);
    return panel;
}

QWidget *RemoteControlPage::buildViewSwapButton()
{
    auto *button = new QPushButton(QStringLiteral("⇄  지도/카메라 전환"));
    button->setObjectName(QStringLiteral("ghostChipButton"));
    connect(button, &QPushButton::clicked, this, &RemoteControlPage::swapCameraAndMap);
    return button;
}

void RemoteControlPage::swapCameraAndMap()
{
    m_mapIsMain = !m_mapIsMain;

    m_bigSlot->removeWidget(m_cameraPanel);
    m_bigSlot->removeWidget(m_minimapPanel);
    m_smallSlot->removeWidget(m_cameraPanel);
    m_smallSlot->removeWidget(m_minimapPanel);

    QWidget *bigWidget = m_mapIsMain ? m_minimapPanel : m_cameraPanel;
    QWidget *smallWidget = m_mapIsMain ? m_cameraPanel : m_minimapPanel;
    m_bigSlot->addWidget(bigWidget);
    m_smallSlot->addWidget(smallWidget);

    if (m_mapIsMain) {
        m_mapView->setMinimumSize(0, 0);
        m_mapView->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        m_mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_minimapPanel->setMinimumHeight(230);
        m_minimapPanel->setMaximumHeight(QWIDGETSIZE_MAX);
        m_minimapPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_cameraPanel->setMinimumHeight(0);
        m_cameraPanel->setFixedHeight(100);
        m_cameraPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    } else {
        m_mapView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_mapView->setMinimumWidth(0);
        m_mapView->setMaximumWidth(QWIDGETSIZE_MAX);
        m_mapView->setFixedHeight(100);
        m_minimapPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        m_minimapPanel->setMinimumHeight(0);
        m_minimapPanel->setMaximumHeight(QWIDGETSIZE_MAX);
        m_cameraPanel->setMaximumHeight(QWIDGETSIZE_MAX);
        m_cameraPanel->setMinimumHeight(230);
        m_cameraPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
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

QWidget *RemoteControlPage::buildSpeedPresetPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(11, 9, 11, 9);
    layout->setSpacing(7);

    auto *headerRow = new QHBoxLayout;
    headerRow->addWidget(makeLabel(QStringLiteral("속도 프리셋"), QStringLiteral("metricCaption")));
    headerRow->addStretch();
    auto *sharedCaption = makeLabel(QStringLiteral("세 방식 값 공유"));
    sharedCaption->setStyleSheet(QStringLiteral("color: %1; font-size: 9px;").arg(DashboardPalette::TextMuted.name()));
    headerRow->addWidget(sharedCaption);
    layout->addLayout(headerRow);

    auto *presetRow = new QHBoxLayout;
    presetRow->setSpacing(5);
    auto *fineButton = new QPushButton(QStringLiteral("미세"));
    fineButton->setObjectName(QStringLiteral("ghostChipButton"));
    auto *normalButton = new QPushButton(QStringLiteral("보통"));
    normalButton->setObjectName(QStringLiteral("cyanChipButton"));
    auto *fastButton = new QPushButton(QStringLiteral("빠름"));
    fastButton->setObjectName(QStringLiteral("ghostChipButton"));
    presetRow->addWidget(fineButton, 1);
    presetRow->addWidget(normalButton, 1);
    presetRow->addWidget(fastButton, 1);
    layout->addLayout(presetRow);

    layout->addWidget(buildSpeedAxisRow(QStringLiteral("▲ 직진"), QStringLiteral("0.12"), QStringLiteral("m/s"), 55));
    layout->addWidget(buildSpeedAxisRow(QStringLiteral("↻ 회전"), QStringLiteral("0.80"), QStringLiteral("rad/s"), 28));

    return panel;
}

QWidget *RemoteControlPage::buildSpeedAxisRow(const QString &label, const QString &value, const QString &unit, int percent)
{
    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *row = new QHBoxLayout;
    auto *labelWidget = makeLabel(label);
    labelWidget->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::TextSecondary.name()));
    row->addWidget(labelWidget);
    row->addStretch();

    auto *valueBox = makeLabel(value);
    valueBox->setAlignment(Qt::AlignCenter);
    valueBox->setFixedSize(50, 22);
    valueBox->setStyleSheet(QStringLiteral("color: %1; font-size: 10px; background-color: %2; border: 1px solid %3; border-radius: 5px;")
                                 .arg(DashboardPalette::TextPrimary.name(), DashboardPalette::CardBg.name(), DashboardPalette::BorderGuide.name()));
    row->addWidget(valueBox);

    auto *unitLabel = makeLabel(unit);
    unitLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 9px;").arg(DashboardPalette::TextTertiary.name()));
    row->addWidget(unitLabel);
    layout->addLayout(row);

    layout->addWidget(makeProportionalBar(percent, DashboardPalette::Cyan, container));
    return container;
}

QWidget *RemoteControlPage::buildCmdVelPanel()
{
    auto *panel = panelFrame(QStringLiteral("callQueueCard"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(11, 8, 11, 8);
    layout->setSpacing(4);

    auto *title = makeLabel(QStringLiteral("발행 중 · /cmd_vel"));
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 9px; letter-spacing: 0.5px;").arg(DashboardPalette::Cyan.name()));
    layout->addWidget(title);

    auto *linearRow = new QHBoxLayout;
    linearRow->addWidget(makeLabel(QStringLiteral("linear.x"), QStringLiteral("metricCaption")));
    linearRow->addStretch();
    auto *linearValue = makeLabel(QStringLiteral("0.00 m/s"));
    linearValue->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::TextPrimary.name()));
    linearRow->addWidget(linearValue);
    layout->addLayout(linearRow);

    auto *angularRow = new QHBoxLayout;
    angularRow->addWidget(makeLabel(QStringLiteral("angular.z"), QStringLiteral("metricCaption")));
    angularRow->addStretch();
    auto *angularValue = makeLabel(QStringLiteral("0.00 rad/s"));
    angularValue->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::TextPrimary.name()));
    angularRow->addWidget(angularValue);
    layout->addLayout(angularRow);

    return panel;
}
