#include "callhistorypage.h"

#include "../theme/dashboardpalette.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
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

struct HistoryRow
{
    QString mission;
    QString summary;
    QString duration;
    QString payload;
    QColor payloadColor;
    QString sync;
    QColor syncColor;
    bool hasReplay;
};
} // namespace

CallHistoryPage::CallHistoryPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildHeader());
    layout->addWidget(buildTable(), 1);
    layout->addWidget(makeLabel(
        QStringLiteral("행 클릭 → 호출·도착·반출·복귀 이벤트 체인 상세 · 재생 클릭 → 주행 기록 화면으로 이동"),
        QStringLiteral("metricCaption")));
}

QWidget *CallHistoryPage::buildHeader()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(QStringLiteral("호출 이력")));
    layout->addStretch();
    for (const QString &filter : {QStringLiteral("기간"), QStringLiteral("호출 구역"), QStringLiteral("결과")}) {
        layout->addWidget(new QPushButton(filter, row));
    }
    return row;
}

QWidget *CallHistoryPage::buildTable()
{
    const QList<HistoryRow> rows = {
        {QStringLiteral("#142"), QStringLiteral("BTN_02 승강장 · 완료"), QStringLiteral("4:12"),
         QStringLiteral("반출됨"), DashboardPalette::Green, QStringLiteral("완료"), DashboardPalette::Green, true},
        {QStringLiteral("#141"), QStringLiteral("BTN_01 대합실 · 3분 타임아웃"), QStringLiteral("7:30"),
         QStringLiteral("미반출"), DashboardPalette::Amber, QStringLiteral("완료"), DashboardPalette::Green, true},
        {QStringLiteral("#140"), QStringLiteral("BTN_03 개찰구 · Nav2 실패"), QStringLiteral("2:05"),
         QStringLiteral("—"), DashboardPalette::TextTertiary, QStringLiteral("대기"), DashboardPalette::Amber, false},
    };

    auto *table = new QTableWidget(rows.size(), 6);
    table->setHorizontalHeaderLabels({QStringLiteral("미션"), QStringLiteral("호출 → 결과"), QStringLiteral("소요"),
                                       QStringLiteral("반출"), QStringLiteral("동기화"), QString()});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    for (int row = 0; row < rows.size(); ++row) {
        const HistoryRow &data = rows.at(row);
        auto *missionItem = new QTableWidgetItem(data.mission);
        missionItem->setForeground(DashboardPalette::Cyan);
        table->setItem(row, 0, missionItem);
        table->setItem(row, 1, new QTableWidgetItem(data.summary));
        table->setItem(row, 2, new QTableWidgetItem(data.duration));
        auto *payloadItem = new QTableWidgetItem(data.payload);
        payloadItem->setForeground(data.payloadColor);
        table->setItem(row, 3, payloadItem);
        auto *syncItem = new QTableWidgetItem(data.sync);
        syncItem->setForeground(data.syncColor);
        table->setItem(row, 4, syncItem);

        if (data.hasReplay) {
            auto *replayButton = new QPushButton(QStringLiteral("▶ 재생"));
            connect(replayButton, &QPushButton::clicked, this, &CallHistoryPage::replayRequested);
            table->setCellWidget(row, 5, replayButton);
        } else {
            auto *noVideoLabel = makeLabel(QStringLiteral("영상 없음"), QStringLiteral("metricCaption"));
            noVideoLabel->setAlignment(Qt::AlignCenter);
            table->setCellWidget(row, 5, noVideoLabel);
        }
    }

    return table;
}
