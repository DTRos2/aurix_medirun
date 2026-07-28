#include "operationsoverviewpage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/proportionalbar.h"

#include <QAbstractItemView>
#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
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

struct RobotRow
{
    QString name;
    QString status;
    QColor statusColor;
    QString battery;
    QString route;
};

struct EventRow
{
    QString time;
    QString message;
    QString severity;
    QString kind;
};
} // namespace

OperationsOverviewPage::OperationsOverviewPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);
    layout->addWidget(buildHeader());

    auto *metricRow = new QGridLayout;
    metricRow->setHorizontalSpacing(12);
    metricRow->setVerticalSpacing(12);
    metricRow->addWidget(buildMetricCard(QStringLiteral("연결 로봇"), QStringLiteral("03"), QStringLiteral("온라인 3 · 점검 0"), QStringLiteral("cyan")), 0, 0);
    metricRow->addWidget(buildMetricCard(QStringLiteral("진행 미션"), QStringLiteral("02"), QStringLiteral("평균 응답 18초"), QStringLiteral("cyan")), 0, 1);
    metricRow->addWidget(buildMetricCard(QStringLiteral("오늘 호출"), QStringLiteral("17"), QStringLiteral("전일 대비 +12%"), QStringLiteral("muted")), 0, 2);
    metricRow->addWidget(buildMetricCard(QStringLiteral("안전 이벤트"), QStringLiteral("01"), QStringLiteral("최근 24시간"), QStringLiteral("amber")), 0, 3);
    layout->addLayout(metricRow);

    auto *midRow = new QHBoxLayout;
    midRow->setSpacing(12);
    midRow->addWidget(buildRobotStatusPanel(), 7);
    midRow->addWidget(buildHealthPanel(), 4);
    layout->addLayout(midRow);

    layout->addWidget(buildEventsPanel());
}

QWidget *OperationsOverviewPage::buildHeader()
{
    auto *header = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(10);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(4);
    titleCol->addWidget(makeLabel(QStringLiteral("운영 현황"), QStringLiteral("pageTitle")));
    titleCol->addWidget(makeLabel(
        QStringLiteral("%1 · AURIX MEDIRUN NETWORK").arg(QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"))),
        QStringLiteral("pageSubtitle")));
    layout->addLayout(titleCol);
    layout->addStretch();
    layout->addWidget(makeBadgeLabel(QStringLiteral("전체 시스템 정상"), QStringLiteral("green"), header));
    return header;
}

QWidget *OperationsOverviewPage::buildMetricCard(const QString &title, const QString &value, const QString &caption, const QString &kind)
{
    auto *card = panelFrame(QStringLiteral("metricCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(makeLabel(title, QStringLiteral("metricTitle")));
    topRow->addStretch();
    topRow->addWidget(makeBadgeLabel(QString(), kind, card));
    layout->addLayout(topRow);

    layout->addWidget(makeLabel(value, QStringLiteral("metricValue")));
    layout->addWidget(makeLabel(caption, QStringLiteral("metricCaption")));
    return card;
}

QWidget *OperationsOverviewPage::buildRobotStatusPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("실시간 로봇 상태"), QStringLiteral("panelTitle")));

    const QList<RobotRow> rows = {
        {QStringLiteral("MR-01"), QStringLiteral("● 출동 중"), DashboardPalette::Cyan, QStringLiteral("86%"), QStringLiteral("B1 중앙홀 → W-03")},
        {QStringLiteral("MR-02"), QStringLiteral("● 대기"), DashboardPalette::TextMuted, QStringLiteral("94%"), QStringLiteral("충전 스테이션 A")},
        {QStringLiteral("MR-03"), QStringLiteral("● 재계획"), DashboardPalette::Amber, QStringLiteral("61%"), QStringLiteral("W-07 → 충전 스테이션")},
    };

    auto *table = new QTableWidget(rows.size(), 4, panel);
    table->setHorizontalHeaderLabels({QStringLiteral("로봇"), QStringLiteral("상태"), QStringLiteral("배터리"), QStringLiteral("경로")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);

    for (int row = 0; row < rows.size(); ++row) {
        const RobotRow &data = rows.at(row);
        table->setItem(row, 0, new QTableWidgetItem(data.name));
        auto *statusItem = new QTableWidgetItem(data.status);
        statusItem->setForeground(data.statusColor);
        table->setItem(row, 1, statusItem);
        table->setItem(row, 2, new QTableWidgetItem(data.battery));
        table->setItem(row, 3, new QTableWidgetItem(data.route));
    }

    layout->addWidget(table);
    return panel;
}

QWidget *OperationsOverviewPage::buildHealthPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("시스템 헬스"), QStringLiteral("panelTitle")));

    struct HealthRow
    {
        QString label;
        int percent;
        QColor color;
    };
    const QList<HealthRow> rows = {
        {QStringLiteral("ROS2 통신"), 96, DashboardPalette::Green},
        {QStringLiteral("Nav2 안정성"), 94, DashboardPalette::Green},
        {QStringLiteral("DB 동기화"), 100, DashboardPalette::Green},
        {QStringLiteral("네트워크 품질"), 87, DashboardPalette::Amber},
    };

    for (const HealthRow &row : rows) {
        auto *rowLayout = new QVBoxLayout;
        rowLayout->setSpacing(6);

        auto *labelRow = new QHBoxLayout;
        labelRow->addWidget(makeLabel(row.label, QStringLiteral("healthLabel")));
        labelRow->addStretch();
        labelRow->addWidget(makeLabel(QStringLiteral("%1%").arg(row.percent), QStringLiteral("healthValue")));
        rowLayout->addLayout(labelRow);
        rowLayout->addWidget(makeProportionalBar(row.percent, row.color, panel));
        layout->addLayout(rowLayout);
    }

    layout->addStretch();
    return panel;
}

QWidget *OperationsOverviewPage::buildEventsPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("최근 운영 이벤트"), QStringLiteral("panelTitle")));

    const QList<EventRow> events = {
        {QStringLiteral("11:43"), QStringLiteral("비상 정지 활성 · MR-01"), QStringLiteral("CRITICAL"), QStringLiteral("red")},
        {QStringLiteral("11:41"), QStringLiteral("경로 재계획 3회 · MR-03"), QStringLiteral("WARN"), QStringLiteral("amber")},
        {QStringLiteral("11:38"), QStringLiteral("미션 상태 ARRIVED 로 변경 · MR-01"), QStringLiteral("INFO"), QStringLiteral("muted")},
    };

    for (const EventRow &event : events) {
        auto *row = new QHBoxLayout;
        row->setSpacing(8);

        auto *bar = new QFrame(panel);
        bar->setFixedSize(3, 20);
        bar->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 1px;")
                                .arg(event.kind == QStringLiteral("red") ? DashboardPalette::Red.name()
                                     : event.kind == QStringLiteral("amber") ? DashboardPalette::Amber.name()
                                                                              : DashboardPalette::TextMuted.name()));
        row->addWidget(bar);
        row->addWidget(makeLabel(event.time, QStringLiteral("metricCaption")));
        row->addWidget(makeLabel(event.message, QStringLiteral("pageSubtitle")), 1);
        row->addWidget(makeBadgeLabel(event.severity, event.kind, panel));
        layout->addLayout(row);
    }

    return panel;
}
