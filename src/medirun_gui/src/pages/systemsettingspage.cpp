#include "systemsettingspage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
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

SystemSettingsPage::SystemSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    auto *headerPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *headerLayout = new QVBoxLayout(headerPanel);
    headerLayout->setContentsMargins(20, 18, 20, 18);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(makeLabel(QStringLiteral("시스템 설정"), QStringLiteral("pageTitle")));
    headerLayout->addWidget(makeLabel(
        QStringLiteral("ROS2, Nav2, 데이터베이스, 네트워크 상태 기준과 대시보드 임계값을 점검합니다."),
        QStringLiteral("pageSubtitle")));
    layout->addWidget(headerPanel);

    const QStringList items = {
        QStringLiteral("ROS2 브리지 · 연결 정상 · 패킷 손실 0.2%"),
        QStringLiteral("Nav2 상태 · Planner/Controller 모두 정상"),
        QStringLiteral("DB 동기화 · 최근 갱신 1초 전"),
        QStringLiteral("알림 임계값 · 배터리 20% · 지연 150ms · Heartbeat 3초")
    };

    auto *listPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(18, 18, 18, 18);
    listLayout->setSpacing(12);
    listLayout->addWidget(makeLabel(QStringLiteral("상세 항목"), QStringLiteral("panelTitle")));

    for (const QString &item : items) {
        auto *row = panelFrame(QStringLiteral("metricCard"));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(16, 14, 16, 14);
        rowLayout->addWidget(makeLabel(item, QStringLiteral("pageSubtitle")));
        listLayout->addWidget(row);
    }

    listLayout->addStretch();
    layout->addWidget(listPanel, 1);
}
