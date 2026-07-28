#include "livecontrolpage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/mapviewwidget.h"
#include "../widgets/proportionalbar.h"

#include <QFrame>
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

struct StepInfo
{
    QString label;
    QString state; // "done" | "active" | "pending"
};
} // namespace

LiveControlPage::LiveControlPage(QWidget *parent)
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

    layout->addWidget(buildMissionStepper());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);

    m_mapView = new MapViewWidget(canvas);
    m_mapView->setMinimumHeight(250);
    m_mapView->setMode(MapViewMode::Live);
    m_mapView->setCaption(QStringLiteral("B1 승강장 · AMCL 정상 · 축척 1m"));
    m_mapView->setRecording(true);
    m_mapView->setRobotPose(QPointF(0.21, 0.76), 0.0);
    m_mapView->setRobotState(RobotMarkerState::Moving);
    m_mapView->setPlannedPath({QPointF(0.21, 0.76), QPointF(0.395, 0.76), QPointF(0.395, 0.4), QPointF(0.705, 0.4)});
    m_mapView->setWaypoints({QPointF(0.395, 0.208), QPointF(0.184, 0.28)});
    m_mapView->setDestination(QPointF(0.705, 0.4));
    m_mapView->setHome(QPointF(0.842, 0.76));
    leftCol->addWidget(m_mapView, 1);
    leftCol->addWidget(buildActionRow());
    bodyRow->addLayout(leftCol, 7);

    auto *rightCol = new QVBoxLayout;
    rightCol->setSpacing(8);
    rightCol->addWidget(buildRobotStatusPanel());
    rightCol->addWidget(buildEventStreamPanel(), 1);
    rightCol->addWidget(buildWaitingCallBanner());
    auto *rightColWidget = new QWidget(canvas);
    rightColWidget->setFixedWidth(200);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    layout->addLayout(bodyRow, 1);
}

QWidget *LiveControlPage::buildMissionStepper()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(9);

    auto *infoRow = new QHBoxLayout;
    auto *missionLabel = makeLabel(QStringLiteral("미션 #142 · BTN_02 동측 승강장"));
    missionLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Cyan.name()));
    infoRow->addWidget(missionLabel);
    infoRow->addStretch();
    infoRow->addWidget(makeLabel(QStringLiteral("경과 00:48 · 남은 12.4m · ETA 00:22"), QStringLiteral("metricCaption")));
    layout->addLayout(infoRow);

    const QList<StepInfo> steps = {
        {QStringLiteral("호출 접수"), QStringLiteral("done")},
        {QStringLiteral("출동 중"), QStringLiteral("active")},
        {QStringLiteral("현장 도착"), QStringLiteral("pending")},
        {QStringLiteral("AED 반출"), QStringLiteral("pending")},
        {QStringLiteral("복귀·도킹"), QStringLiteral("pending")},
    };

    auto *stepRow = new QHBoxLayout;
    stepRow->setSpacing(0);
    for (int i = 0; i < steps.size(); ++i) {
        const StepInfo &step = steps.at(i);
        auto *col = new QVBoxLayout;
        col->setSpacing(4);
        auto *circle = new QLabel(step.state == QStringLiteral("done") ? QStringLiteral("✓") : QString());
        circle->setProperty("stepState", step.state);
        circle->setFixedSize(18, 18);
        circle->setAlignment(Qt::AlignCenter);
        col->addWidget(circle, 0, Qt::AlignHCenter);
        auto *stepLabel = makeLabel(step.label, QStringLiteral("metricCaption"));
        stepLabel->setAlignment(Qt::AlignHCenter);
        col->addWidget(stepLabel);
        stepRow->addLayout(col, 10);

        if (i < steps.size() - 1) {
            auto *connector = new QFrame(panel);
            connector->setFixedHeight(2);
            connector->setObjectName(step.state == QStringLiteral("done")
                                          ? QStringLiteral("stepConnectorDone")
                                          : QStringLiteral("stepConnectorPending"));
            stepRow->addWidget(connector, 6, Qt::AlignVCenter);
        }
    }
    layout->addLayout(stepRow);
    return panel;
}

QWidget *LiveControlPage::buildActionRow()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    auto *recordButton = new QPushButton(QStringLiteral("녹화 중지"));
    auto *snapshotButton = new QPushButton(QStringLiteral("스냅샷"));
    auto *cancelButton = new QPushButton(QStringLiteral("미션 취소"));
    auto *homeButton = new QPushButton(QStringLiteral("홈 복귀"));

    for (QPushButton *button : {recordButton, snapshotButton, cancelButton, homeButton}) {
        layout->addWidget(button);
    }

    connect(cancelButton, &QPushButton::clicked, this, &LiveControlPage::missionCancelRequested);
    connect(homeButton, &QPushButton::clicked, this, &LiveControlPage::returnHomeRequested);
    return row;
}

QWidget *LiveControlPage::buildRobotStatusPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("로봇 상태"), QStringLiteral("metricCaption")));

    statRow(layout, QStringLiteral("배터리"), QStringLiteral("76%"));
    layout->addWidget(makeProportionalBar(76, DashboardPalette::Green, panel));
    layout->addSpacing(4);
    statRow(layout, QStringLiteral("AED 적재"), QStringLiteral("적재됨"), DashboardPalette::Green);
    statRow(layout, QStringLiteral("전방 장애물"), QStringLiteral("42cm"));
    statRow(layout, QStringLiteral("Nav2"), QStringLiteral("주행중"));
    statRow(layout, QStringLiteral("속도"), QStringLiteral("0.18 m/s"));
    return panel;
}

QWidget *LiveControlPage::buildEventStreamPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("이벤트 스트림"), QStringLiteral("metricCaption")));

    struct StreamEvent
    {
        QString type;
        QString detail;
        QColor color;
    };
    const QList<StreamEvent> events = {
        {QStringLiteral("NAV_RETRY"), QStringLiteral("14:02:39 경로 재계획"), DashboardPalette::Amber},
        {QStringLiteral("DISPATCHED"), QStringLiteral("14:02:12 출동 시작"), DashboardPalette::TextMuted},
    };

    for (const StreamEvent &event : events) {
        auto *row = new QHBoxLayout;
        row->setSpacing(6);
        auto *bar = new QFrame(panel);
        bar->setFixedSize(3, 26);
        bar->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 1px;").arg(event.color.name()));
        row->addWidget(bar);

        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(1);
        auto *typeLabel = makeLabel(event.type);
        typeLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(event.color.name()));
        textCol->addWidget(typeLabel);
        textCol->addWidget(makeLabel(event.detail, QStringLiteral("metricCaption")));
        row->addLayout(textCol, 1);
        layout->addLayout(row);
    }

    layout->addStretch();
    return panel;
}

QWidget *LiveControlPage::buildWaitingCallBanner()
{
    auto *panel = panelFrame(QStringLiteral("callQueueCard"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(11, 8, 11, 8);
    auto *label = makeLabel(QStringLiteral("대기 호출 1건"));
    label->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Cyan.name()));
    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(makeLabel(QStringLiteral("미션 관제 →"), QStringLiteral("metricCaption")));
    return panel;
}
