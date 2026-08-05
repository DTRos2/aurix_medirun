#include "sidenav.h"

#include "badgelabel.h"

#include <QFrame>
#include <QHBoxLayout>
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

    sections.append({QStringLiteral("관제"), {
        {PageId::OperationsOverview, QStringLiteral("운영 현황")},
        {PageId::LiveControl, QStringLiteral("실시간 관제")},
        {PageId::MissionOps, QStringLiteral("미션 관제")},
        {PageId::RemoteControl, QStringLiteral("원격 제어")},
    }});

    if (role == UserRole::Admin) {
        sections.append({QStringLiteral("지도"), {
            {PageId::SlamMapping, QStringLiteral("지도 생성")},
            {PageId::WaypointManagement, QStringLiteral("웨이포인트 관리")},
        }});
    }

    sections.append({QStringLiteral("기록"), {
        {PageId::RecordsPlayback, QStringLiteral("주행 기록")},
        {PageId::CallHistory, QStringLiteral("호출 이력")},
    }});

    if (role == UserRole::Admin) {
        sections.append({QStringLiteral("시스템"), {
            {PageId::UserManagement, QStringLiteral("사용자 관리")},
            {PageId::RobotManagement, QStringLiteral("로봇 관리")},
            {PageId::SystemSettings, QStringLiteral("시스템 설정")},
        }});
    }

    return sections;
}

QFrame *horizontalDivider(QWidget *parent)
{
    auto *divider = new QFrame(parent);
    divider->setObjectName(QStringLiteral("sectionDivider"));
    divider->setFixedHeight(1);
    return divider;
}

} // namespace

SideNav::SideNav(const AppSession &session, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("sideNav"));
    setFixedWidth(200);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 18, 14, 18);
    layout->setSpacing(6);

    const QList<NavSection> sections = sectionsForRole(session.role);
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
    layout->addWidget(buildAccountFooter(session));
}

QWidget *SideNav::buildAccountFooter(const AppSession &session)
{
    auto *footer = new QWidget(this);
    auto *layout = new QVBoxLayout(footer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(horizontalDivider(footer));

    auto *userRow = new QHBoxLayout;
    userRow->setSpacing(8);

    auto *userNameLabel = new QLabel(session.username.isEmpty() ? QStringLiteral("--") : session.username, footer);
    userNameLabel->setObjectName(QStringLiteral("sidebarUserName"));
    userRow->addWidget(userNameLabel, 1);

    auto *roleBadge = makeBadgeLabel(roleDisplayName(session.role),
                                      session.role == UserRole::Admin ? QStringLiteral("cyan") : QStringLiteral("muted"),
                                      footer);
    userRow->addWidget(roleBadge);
    layout->addLayout(userRow);

    auto *logoutButton = new QPushButton(QStringLiteral("로그아웃"), footer);
    logoutButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(logoutButton, &QPushButton::clicked, this, &SideNav::logoutRequested);
    layout->addWidget(logoutButton);

    auto *exitButton = new QPushButton(QStringLiteral("✕  대시보드 종료"), footer);
    exitButton->setObjectName(QStringLiteral("inlineGhostButton"));
    exitButton->setToolTip(QStringLiteral("대시보드를 종료합니다"));
    connect(exitButton, &QPushButton::clicked, this, &SideNav::exitRequested);
    layout->addWidget(exitButton);

    return footer;
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
