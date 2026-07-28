#include "missionopspage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
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
} // namespace

MissionOpsPage::MissionOpsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildAnomalyCard());

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(8);
    leftCol->addWidget(makeLabel(QStringLiteral("대기 호출 큐 · 1건"), QStringLiteral("metricCaption")));
    leftCol->addWidget(buildWaitingQueue());
    leftCol->addWidget(makeLabel(QStringLiteral("진행 중 미션"), QStringLiteral("metricCaption")));
    leftCol->addWidget(buildInProgressMission());
    auto *leftWidget = new QWidget(canvas);
    leftWidget->setLayout(leftCol);
    bodyRow->addWidget(leftWidget, 5);

    bodyRow->addWidget(buildTodayHistory(), 6);
    layout->addLayout(bodyRow, 1);
}

QWidget *MissionOpsPage::buildAnomalyCard()
{
    auto *panel = panelFrame(QStringLiteral("anomalyCard"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(13, 11, 13, 11);
    layout->setSpacing(8);

    auto *headerRow = new QHBoxLayout;
    auto *title = makeLabel(QStringLiteral("⚠ 이상 상황 · 조치 필요"));
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; font-weight: 600;").arg(DashboardPalette::Red.name()));
    headerRow->addWidget(title);
    headerRow->addStretch();
    headerRow->addWidget(makeLabel(QStringLiteral("14:04:11 · 미션 #142 · W-05 부근"), QStringLiteral("metricCaption")));
    layout->addLayout(headerRow);

    layout->addWidget(makeLabel(QStringLiteral("NAV_PLAN_FAILED — 경로 재계획 3회 연속 실패")));
    layout->addWidget(makeLabel(
        QStringLiteral("전방 0.21m 장애물 지속 · 배터리 76% · AMCL 정상 · Nav2 controller 응답 있음"),
        QStringLiteral("metricCaption")));

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(6);
    auto *retryButton = new QPushButton(QStringLiteral("재시도"));
    retryButton->setObjectName(QStringLiteral("primaryCtaButton"));
    auto *homeButton = new QPushButton(QStringLiteral("홈 복귀"));
    auto *cancelButton = new QPushButton(QStringLiteral("미션 취소"));
    auto *remoteButton = new QPushButton(QStringLiteral("원격 제어 →"));
    for (QPushButton *button : {retryButton, homeButton, cancelButton, remoteButton}) {
        buttonRow->addWidget(button);
    }
    layout->addLayout(buttonRow);

    connect(homeButton, &QPushButton::clicked, this, &MissionOpsPage::returnHomeRequested);
    connect(cancelButton, &QPushButton::clicked, this, &MissionOpsPage::missionCancelRequested);
    connect(remoteButton, &QPushButton::clicked, this, &MissionOpsPage::remoteControlRequested);
    return panel;
}

QWidget *MissionOpsPage::buildWaitingQueue()
{
    auto *panel = panelFrame(QStringLiteral("callQueueCard"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(makeLabel(QStringLiteral("BTN_01 · 중앙 대합실")));
    titleRow->addWidget(makeBadgeLabel(QStringLiteral("긴급"), QStringLiteral("red"), panel));
    titleRow->addStretch();
    layout->addLayout(titleRow);

    layout->addWidget(makeLabel(QStringLiteral("14:03:52 접수 · 대기 00:19 · 현재 미션 대비 거리 32m"), QStringLiteral("metricCaption")));

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(6);
    auto *acceptButton = new QPushButton(QStringLiteral("확인·전환"));
    acceptButton->setObjectName(QStringLiteral("primaryCtaButton"));
    auto *holdButton = new QPushButton(QStringLiteral("보류"));
    auto *cancelButton = new QPushButton(QStringLiteral("취소"));
    for (QPushButton *button : {acceptButton, holdButton, cancelButton}) {
        buttonRow->addWidget(button);
    }
    layout->addLayout(buttonRow);
    return panel;
}

QWidget *MissionOpsPage::buildInProgressMission()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(makeLabel(QStringLiteral("#142 · BTN_02 동측 승강장")));
    titleRow->addStretch();
    auto *stateLabel = makeLabel(QStringLiteral("출동 중"));
    stateLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;").arg(DashboardPalette::Cyan.name()));
    titleRow->addWidget(stateLabel);
    layout->addLayout(titleRow);

    layout->addWidget(makeProportionalBar(38, DashboardPalette::Cyan, panel));
    layout->addWidget(makeLabel(QStringLiteral("TB3-01 · 남은 12.4m · 경과 00:48"), QStringLiteral("metricCaption")));
    return panel;
}

QWidget *MissionOpsPage::buildTodayHistory()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 11, 12, 11);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(QStringLiteral("오늘 처리 이력"), QStringLiteral("metricCaption")));

    struct HistoryRow
    {
        QString time;
        QString title;
        QString detail;
    };
    const QList<HistoryRow> rows = {
        {QStringLiteral("13:41"), QStringLiteral("#141 보류 → 취소"), QStringLiteral("admin_lee · 3분 미반출")},
        {QStringLiteral("13:12"), QStringLiteral("#140 재시도 → 성공"), QStringLiteral("kim · 도킹 실패 후 복구")},
        {QStringLiteral("11:58"), QStringLiteral("#139 정상 완료"), QStringLiteral("개입 없음")},
    };

    for (const HistoryRow &row : rows) {
        auto *rowLayout = new QHBoxLayout;
        rowLayout->setSpacing(7);
        auto *timeLabel = makeLabel(row.time, QStringLiteral("metricCaption"));
        timeLabel->setFixedWidth(36);
        rowLayout->addWidget(timeLabel);
        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(1);
        textCol->addWidget(makeLabel(row.title));
        textCol->addWidget(makeLabel(row.detail, QStringLiteral("metricCaption")));
        rowLayout->addLayout(textCol, 1);
        layout->addLayout(rowLayout);
    }

    layout->addStretch();
    layout->addWidget(makeLabel(QStringLiteral("모든 판단은 사용자 ID와 함께 DB에 기록됩니다."), QStringLiteral("metricCaption")));
    return panel;
}
