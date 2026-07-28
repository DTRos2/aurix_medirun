#include "sidenav.h"

#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

struct NavEntry
{
    PageId id;
    QString label;
};

struct NavSection
{
    QString title;
    QList<NavEntry> entries;
};

QList<NavSection> sectionsForRole(UserRole role)
{
    QList<NavSection> sections;

    if (role == UserRole::Admin) {
        sections.append({QStringLiteral("관제"), {
            {PageId::OperationsOverview, QStringLiteral("운영 현황")},
            {PageId::LiveControl, QStringLiteral("실시간 관제")},
            {PageId::MissionOps, QStringLiteral("미션 관제")},
            {PageId::RemoteControl, QStringLiteral("원격 제어")},
        }});

        sections.append({QStringLiteral("지도"), {
            {PageId::SlamMapping, QStringLiteral("지도 생성")},
        }});
    } else {
        sections.append({QStringLiteral("관제"), {
            {PageId::LiveControl, QStringLiteral("실시간 관제")},
            {PageId::MissionOps, QStringLiteral("미션 관제")},
        }});
    }

    sections.append({QStringLiteral("기록"), {
        {PageId::RecordsPlayback, QStringLiteral("주행 기록")},
        {PageId::CallHistory, QStringLiteral("호출 이력")},
    }});

    if (role == UserRole::Admin) {
        sections.append({QStringLiteral("시스템"), {
            {PageId::UserManagement, QStringLiteral("사용자 관리")},
            {PageId::SystemSettings, QStringLiteral("시스템 설정")},
        }});
    }

    return sections;
}

} // namespace

SideNav::SideNav(UserRole role, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("sideNav"));
    setFixedWidth(200);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 18, 14, 18);
    layout->setSpacing(6);

    const QList<NavSection> sections = sectionsForRole(role);
    for (const NavSection &section : sections) {
        auto *kicker = new QLabel(section.title, this);
        kicker->setObjectName(QStringLiteral("sectionKicker"));
        layout->addSpacing(6);
        layout->addWidget(kicker);

        for (const NavEntry &entry : section.entries) {
            auto *button = new QPushButton(entry.label, this);
            button->setObjectName(QStringLiteral("sidebarButton"));
            layout->addWidget(button);
            m_buttons.insert(entry.id, button);
            connect(button, &QPushButton::clicked, this, [this, id = entry.id]() {
                emit pageSelected(id);
            });
        }
    }

    layout->addStretch(1);
}

void SideNav::setActivePage(PageId page)
{
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        QPushButton *button = it.value();
        button->setObjectName(it.key() == page
                                   ? QStringLiteral("sidebarButtonActive")
                                   : QStringLiteral("sidebarButton"));
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}
