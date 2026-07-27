#include "dashboardcentralwidget.h"

#include "../services/authservice.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDate>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QFrame *panelFrame(const QString &objectName)
{
    auto *frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

QLabel *makeLabel(const QString &text, const QString &objectName = {})
{
    auto *label = new QLabel(text);
    if (!objectName.isEmpty()) {
        label->setObjectName(objectName);
    }
    label->setWordWrap(true);
    return label;
}

QLabel *makeBrandLogoLabel(QWidget *parent, const QSize &targetSize)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("brandLogo"));
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(targetSize);
    label->setMaximumSize(targetSize);

    const QString logoPath = QStringLiteral(":/images/aurix_logo_01.png");
    const QPixmap logo(logoPath);
    if (logo.isNull()) {
        qWarning() << "Dashboard logo load failed:" << logoPath << "exists:" << QFile::exists(logoPath);
        label->setText(QStringLiteral("A"));
        return label;
    }

    label->setPixmap(logo.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    return label;
}

QLabel *makeRobotVisualLabel(QWidget *parent, const QSize &targetSize)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("robotVisual"));
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(targetSize);

    const QString robotPath = QStringLiteral(":/images/aurix_robot_reference.png");
    const QPixmap poster(robotPath);
    if (poster.isNull()) {
        qWarning() << "Robot reference image load failed:" << robotPath << "exists:" << QFile::exists(robotPath);
        label->setText(QStringLiteral("⬡"));
        label->setObjectName(QStringLiteral("robotHexGlyph"));
        return label;
    }

    const QRect cropRect(
        poster.width() * 0.22,
        poster.height() * 0.12,
        poster.width() * 0.58,
        poster.height() * 0.46);
    const QPixmap croppedRobot = poster.copy(cropRect);
    label->setPixmap(croppedRobot.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    return label;
}
}

DashboardCentralWidget::DashboardCentralWidget(const AppSession &session, AuthService &authService, QWidget *parent)
    : QWidget(parent)
    , m_authService(authService)
    , m_session(session)
    , m_rootLayout(new QVBoxLayout(this))
    , m_adminTitleLabel(nullptr)
    , m_adminDateLabel(nullptr)
    , m_adminSystemBadge(nullptr)
    , m_adminTotalRobotsValue(nullptr)
    , m_adminActiveMissionValue(nullptr)
    , m_adminTodayCallsValue(nullptr)
    , m_adminSafetyEventsValue(nullptr)
    , m_adminStack(nullptr)
    , m_adminRobotTable(nullptr)
    , m_adminHealthRosValue(nullptr)
    , m_adminHealthNavValue(nullptr)
    , m_adminHealthDbValue(nullptr)
    , m_adminHealthNetValue(nullptr)
    , m_adminEventsTable(nullptr)
    , m_userCreateEmailEdit(nullptr)
    , m_userCreatePasswordEdit(nullptr)
    , m_userCreateRoleCombo(nullptr)
    , m_userCreateStatusLabel(nullptr)
    , m_userManagementTable(nullptr)
    , m_userOnlineBadge(nullptr)
    , m_userDescriptionLabel(nullptr)
    , m_userBatteryValue(nullptr)
    , m_userStateValue(nullptr)
    , m_userMissionCountValue(nullptr)
    , m_userRobotIdLabel(nullptr)
    , m_userRobotSubtitleLabel(nullptr)
    , m_userRobotStatusLabel(nullptr)
    , m_userHeartbeatLabel(nullptr)
    , m_userHealthTagsLabel(nullptr)
    , m_userMissionLogLabel(nullptr)
    , m_manualControlButton(nullptr)
    , m_waypointOneButton(nullptr)
    , m_waypointTwoButton(nullptr)
    , m_waypointThreeButton(nullptr)
{
    setObjectName(QStringLiteral("dashboardRoot"));
    m_rootLayout->setContentsMargins(18, 18, 18, 18);
    m_rootLayout->setSpacing(0);

    if (m_session.role == UserRole::Admin) {
        m_rootLayout->addWidget(buildAdminView());
    } else {
        m_rootLayout->addWidget(buildUserView());
    }
}

void DashboardCentralWidget::applySnapshot(const TelemetrySnapshot &snapshot)
{
    if (m_session.role == UserRole::Admin) {
        updateAdminSnapshot(snapshot);
    } else {
        updateUserSnapshot(snapshot);
    }
}

QWidget *DashboardCentralWidget::buildAdminView()
{
    auto *shell = panelFrame(QStringLiteral("pageShell"));
    auto *shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(buildAdminSidebar(), 0);

    auto *content = panelFrame(QStringLiteral("adminContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 22, 24, 22);
    contentLayout->setSpacing(0);

    m_adminStack = new QStackedWidget(content);
    m_adminStack->setObjectName(QStringLiteral("adminStack"));
    m_adminStack->addWidget(buildAdminOverviewPage());
    m_adminStack->addWidget(buildAdminRobotsPage());
    m_adminStack->addWidget(buildAdminMissionsPage());
    m_adminStack->addWidget(buildAdminHistoryPage());
    m_adminStack->addWidget(buildAdminUsersPage());
    m_adminStack->addWidget(buildAdminSettingsPage());
    contentLayout->addWidget(m_adminStack);

    shellLayout->addWidget(content, 1);
    switchAdminSection(0);
    return shell;
}

QWidget *DashboardCentralWidget::buildAdminSidebar()
{
    auto *sidebar = panelFrame(QStringLiteral("adminSidebar"));
    sidebar->setFixedWidth(220);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *brandA = makeBrandLogoLabel(sidebar, QSize(40, 40));
    auto *brandTitle = makeLabel(QStringLiteral("AURIX"), QStringLiteral("brandTitle"));
    auto *brandSubtitle = makeLabel(QStringLiteral("MEDIRUN CONTROL"), QStringLiteral("brandSubtitle"));
    auto *brandRow = new QHBoxLayout;
    brandRow->addWidget(brandA);
    auto *brandTextCol = new QVBoxLayout;
    brandTextCol->setSpacing(0);
    brandTextCol->addWidget(brandTitle);
    brandTextCol->addWidget(brandSubtitle);
    brandRow->addLayout(brandTextCol);
    brandRow->addStretch();
    layout->addLayout(brandRow);
    layout->addSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("CONTROL CENTER"), QStringLiteral("sectionKicker")));

    const QStringList items = {
        QStringLiteral("운영 개요"),
        QStringLiteral("로봇 관리"),
        QStringLiteral("미션 관제"),
        QStringLiteral("호출 이력"),
        QStringLiteral("사용자 관리"),
        QStringLiteral("시스템 설정")
    };
    for (int index = 0; index < items.size(); ++index) {
        auto *button = new QPushButton(items.at(index));
        button->setObjectName(QStringLiteral("sidebarButton"));
        layout->addWidget(button);
        m_adminNavButtons.append(button);
        connect(button, &QPushButton::clicked, this, [this, index]() { switchAdminSection(index); });
    }

    layout->addStretch();

    auto *profile = panelFrame(QStringLiteral("profileCard"));
    auto *profileLayout = new QVBoxLayout(profile);
    profileLayout->setContentsMargins(12, 12, 12, 12);
    profileLayout->setSpacing(2);
    profileLayout->addWidget(makeLabel(QStringLiteral("AD"), QStringLiteral("profileAvatar")));
    profileLayout->addWidget(makeLabel(QStringLiteral("시스템 관리자"), QStringLiteral("profileName")));
    profileLayout->addWidget(makeLabel(QStringLiteral("ADMIN · ONLINE"), QStringLiteral("profileRole")));
    layout->addWidget(profile);
    return sidebar;
}

QWidget *DashboardCentralWidget::buildAdminOverviewPage()
{
    auto *page = new QWidget;
    auto *contentLayout = new QVBoxLayout(page);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(18);
    contentLayout->addWidget(buildAdminHeader());

    auto *metricRow = new QGridLayout;
    metricRow->setHorizontalSpacing(12);
    metricRow->setVerticalSpacing(12);
    metricRow->addWidget(buildAdminMetricCard(QStringLiteral("연결 로봇"), m_adminTotalRobotsValue, QStringLiteral("cyan"), QStringLiteral("온라인 3 · 점검 0")), 0, 0);
    metricRow->addWidget(buildAdminMetricCard(QStringLiteral("진행 미션"), m_adminActiveMissionValue, QStringLiteral("blue"), QStringLiteral("평균 응답 18초")), 0, 1);
    metricRow->addWidget(buildAdminMetricCard(QStringLiteral("오늘 호출"), m_adminTodayCallsValue, QStringLiteral("amber"), QStringLiteral("전일 대비 +12%")), 0, 2);
    metricRow->addWidget(buildAdminMetricCard(QStringLiteral("안전 이벤트"), m_adminSafetyEventsValue, QStringLiteral("green"), QStringLiteral("최근 24시간 정상")), 0, 3);
    contentLayout->addLayout(metricRow);

    auto *midRow = new QHBoxLayout;
    midRow->setSpacing(12);
    midRow->addWidget(buildAdminRobotOverview(), 7);
    midRow->addWidget(buildAdminHealthPanel(), 4);
    contentLayout->addLayout(midRow);
    contentLayout->addWidget(buildAdminEventsPanel());
    return page;
}

QWidget *DashboardCentralWidget::buildAdminRobotsPage()
{
    return buildAdminInfoPage(
        QStringLiteral("로봇 관리"),
        QStringLiteral("운영 중인 로봇의 연결 상태, 배터리, 위치, 미션 상황을 개별 단위로 점검합니다."),
        {
            QStringLiteral("MR-01 · ONLINE · 배터리 86% · B1 중앙홀 순찰 중"),
            QStringLiteral("MR-02 · READY · 배터리 94% · 충전 스테이션 A 대기"),
            QStringLiteral("MR-03 · RETURNING · 배터리 61% · W-07에서 복귀 중"),
            QStringLiteral("마지막 Heartbeat 1초 전 · 네트워크 평균 지연 24ms")
        });
}

QWidget *DashboardCentralWidget::buildAdminMissionsPage()
{
    return buildAdminInfoPage(
        QStringLiteral("미션 관제"),
        QStringLiteral("현재 출동 요청과 자동 복귀, 배송, 순찰 미션을 우선순위 기준으로 모니터링합니다."),
        {
            QStringLiteral("긴급 호출 #E-204 · MR-01 출동 중 · ETA 00:18"),
            QStringLiteral("약품 운반 #M-118 · MR-03 복귀 대기 · 경로 재탐색 완료"),
            QStringLiteral("정기 순찰 #P-041 · MR-02 예약됨 · 17:30 시작"),
            QStringLiteral("미션 취소/재할당/수동 제어는 다음 단계에서 실제 제어 API와 연동 예정")
        });
}

QWidget *DashboardCentralWidget::buildAdminHistoryPage()
{
    return buildAdminInfoPage(
        QStringLiteral("호출 이력"),
        QStringLiteral("최근 호출과 운행 내역을 시간순으로 검토하고 응답 품질을 비교할 수 있습니다."),
        {
            QStringLiteral("16:22 · 응급 호출 수락 · MR-01 · 현장 도착까지 18초"),
            QStringLiteral("15:47 · 약품 배송 완료 · MR-03 · 성공"),
            QStringLiteral("14:08 · 스테이션 복귀 완료 · MR-02 · 성공"),
            QStringLiteral("13:41 · 네트워크 지연 경고 · MR-03 · 자동 복구")
        });
}

QWidget *DashboardCentralWidget::buildAdminUsersPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *headerPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *headerLayout = new QVBoxLayout(headerPanel);
    headerLayout->setContentsMargins(20, 18, 20, 18);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(makeLabel(QStringLiteral("사용자 관리"), QStringLiteral("pageTitle")));
    headerLayout->addWidget(makeLabel(QStringLiteral("이메일 또는 아이디, 비밀번호, 권한을 등록하고 즉시 로그인 가능한 계정을 생성합니다."), QStringLiteral("pageSubtitle")));
    layout->addWidget(headerPanel);

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(14);
    bodyRow->addWidget(buildUserManagementForm(), 4);

    auto *listPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(18, 18, 18, 18);
    listLayout->setSpacing(12);
    listLayout->addWidget(makeLabel(QStringLiteral("등록된 사용자"), QStringLiteral("panelTitle")));
    m_userManagementTable = new QTableWidget(listPanel);
    m_userManagementTable->setColumnCount(2);
    m_userManagementTable->setHorizontalHeaderLabels({QStringLiteral("이메일/아이디"), QStringLiteral("권한")});
    m_userManagementTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_userManagementTable->verticalHeader()->setVisible(false);
    m_userManagementTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userManagementTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userManagementTable->setSelectionMode(QAbstractItemView::SingleSelection);
    listLayout->addWidget(m_userManagementTable, 1);
    bodyRow->addWidget(listPanel, 6);

    layout->addLayout(bodyRow, 1);
    refreshUserTable();
    return page;
}

QWidget *DashboardCentralWidget::buildAdminSettingsPage()
{
    return buildAdminInfoPage(
        QStringLiteral("시스템 설정"),
        QStringLiteral("ROS2, Nav2, 데이터베이스, 네트워크 상태 기준과 대시보드 임계값을 점검합니다."),
        {
            QStringLiteral("ROS2 브리지 · 연결 정상 · 패킷 손실 0.2%"),
            QStringLiteral("Nav2 상태 · Planner/Controller 모두 정상"),
            QStringLiteral("DB 동기화 · 최근 갱신 1초 전"),
            QStringLiteral("알림 임계값 · 배터리 20% · 지연 150ms · Heartbeat 3초")
        });
}

QWidget *DashboardCentralWidget::buildAdminHeader()
{
    auto *header = new QWidget;
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(4);
    m_adminTitleLabel = makeLabel(QStringLiteral("운영 통합 관제"), QStringLiteral("pageTitle"));
    m_adminDateLabel = makeLabel(QStringLiteral("%1 · AURIX MEDIRUN NETWORK").arg(QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"))), QStringLiteral("pageSubtitle"));
    titleCol->addWidget(m_adminTitleLabel);
    titleCol->addWidget(m_adminDateLabel);
    layout->addLayout(titleCol);
    layout->addStretch();

    auto *exitButton = new QPushButton(QStringLiteral("대시보드 종료"));
    exitButton->setObjectName(QStringLiteral("inlineGhostButton"));
    layout->addWidget(exitButton);

    auto *logoutButton = new QPushButton(QStringLiteral("로그아웃"));
    logoutButton->setObjectName(QStringLiteral("inlineGhostButton"));
    layout->addWidget(logoutButton);

    m_adminSystemBadge = makeLabel(QStringLiteral("전체 시스템 정상"), QStringLiteral("statusBadgeGreen"));
    layout->addWidget(m_adminSystemBadge);
    connect(exitButton, &QPushButton::clicked, this, &DashboardCentralWidget::exitRequested);
    connect(logoutButton, &QPushButton::clicked, this, &DashboardCentralWidget::logoutRequested);
    return header;
}

QWidget *DashboardCentralWidget::buildAdminInfoPage(const QString &title,
                                                    const QString &subtitle,
                                                    const QStringList &items)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *headerPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *headerLayout = new QHBoxLayout(headerPanel);
    headerLayout->setContentsMargins(20, 18, 20, 18);
    headerLayout->setSpacing(10);
    auto *headerTextLayout = new QVBoxLayout;
    headerTextLayout->setSpacing(6);
    headerTextLayout->addWidget(makeLabel(title, QStringLiteral("pageTitle")));
    headerTextLayout->addWidget(makeLabel(subtitle, QStringLiteral("pageSubtitle")));
    headerLayout->addLayout(headerTextLayout);
    headerLayout->addStretch();
    auto *exitButton = new QPushButton(QStringLiteral("대시보드 종료"));
    exitButton->setObjectName(QStringLiteral("inlineGhostButton"));
    headerLayout->addWidget(exitButton);
    auto *logoutButton = new QPushButton(QStringLiteral("로그아웃"));
    logoutButton->setObjectName(QStringLiteral("inlineGhostButton"));
    headerLayout->addWidget(logoutButton);
    connect(exitButton, &QPushButton::clicked, this, &DashboardCentralWidget::exitRequested);
    connect(logoutButton, &QPushButton::clicked, this, &DashboardCentralWidget::logoutRequested);
    layout->addWidget(headerPanel);

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
    return page;
}

QWidget *DashboardCentralWidget::buildUserManagementForm()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("계정 추가"), QStringLiteral("panelTitle")));
    layout->addWidget(makeLabel(QStringLiteral("관리자가 추가한 계정은 즉시 로그인에 사용할 수 있습니다."), QStringLiteral("pageSubtitle")));

    auto *emailLabel = makeLabel(QStringLiteral("이메일 또는 아이디"), QStringLiteral("fieldLabel"));
    layout->addWidget(emailLabel);
    m_userCreateEmailEdit = new QLineEdit(panel);
    m_userCreateEmailEdit->setPlaceholderText(QStringLiteral("operator@aurix.local"));
    layout->addWidget(m_userCreateEmailEdit);

    auto *passwordLabel = makeLabel(QStringLiteral("비밀번호"), QStringLiteral("fieldLabel"));
    layout->addWidget(passwordLabel);
    m_userCreatePasswordEdit = new QLineEdit(panel);
    m_userCreatePasswordEdit->setPlaceholderText(QStringLiteral("8자 이상"));
    m_userCreatePasswordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_userCreatePasswordEdit);

    auto *roleLabel = makeLabel(QStringLiteral("권한"), QStringLiteral("fieldLabel"));
    layout->addWidget(roleLabel);
    m_userCreateRoleCombo = new QComboBox(panel);
    m_userCreateRoleCombo->addItem(QStringLiteral("USER"), static_cast<int>(UserRole::Staff));
    m_userCreateRoleCombo->addItem(QStringLiteral("ADMIN"), static_cast<int>(UserRole::Admin));
    layout->addWidget(m_userCreateRoleCombo);

    auto *createButton = new QPushButton(QStringLiteral("사용자 추가"));
    createButton->setObjectName(QStringLiteral("primaryCtaButton"));
    layout->addWidget(createButton);

    m_userCreateStatusLabel = makeLabel(QStringLiteral("기본 계정 admin/user 외에도 신규 계정을 등록할 수 있습니다."), QStringLiteral("pageSubtitle"));
    layout->addWidget(m_userCreateStatusLabel);
    layout->addStretch();

    connect(createButton, &QPushButton::clicked, this, &DashboardCentralWidget::handleCreateUser);
    connect(m_userCreatePasswordEdit, &QLineEdit::returnPressed, this, &DashboardCentralWidget::handleCreateUser);
    return panel;
}

QWidget *DashboardCentralWidget::buildAdminMetricCard(const QString &title, QLabel *&valueLabel, const QString &accentName, const QString &caption)
{
    auto *card = panelFrame(QStringLiteral("metricCard"));
    card->setProperty("accent", accentName);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(makeLabel(title, QStringLiteral("metricTitle")));
    topRow->addStretch();
    auto *dot = makeLabel(QStringLiteral("•"), QStringLiteral("metricDot"));
    dot->setProperty("accent", accentName);
    topRow->addWidget(dot);
    layout->addLayout(topRow);

    valueLabel = makeLabel(QStringLiteral("--"), QStringLiteral("metricValue"));
    layout->addWidget(valueLabel);
    layout->addWidget(makeLabel(caption, QStringLiteral("metricCaption")));
    return card;
}

QWidget *DashboardCentralWidget::buildAdminRobotOverview()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("실시간 로봇 상태"), QStringLiteral("panelTitle")));

    m_adminRobotTable = new QTableWidget(3, 4, panel);
    m_adminRobotTable->setObjectName(QStringLiteral("robotOverviewTable"));
    m_adminRobotTable->setHorizontalHeaderLabels({
        QStringLiteral("로봇"),
        QStringLiteral("상태"),
        QStringLiteral("배터리"),
        QStringLiteral("경로")
    });
    m_adminRobotTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_adminRobotTable->verticalHeader()->setVisible(false);
    m_adminRobotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_adminRobotTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_adminRobotTable->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_adminRobotTable);
    return panel;
}

QWidget *DashboardCentralWidget::buildAdminHealthPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("시스템 헬스"), QStringLiteral("panelTitle")));

    const QList<QPair<QString, QLabel **>> rows = {
        {QStringLiteral("ROS2 통신"), &m_adminHealthRosValue},
        {QStringLiteral("Nav2 안정성"), &m_adminHealthNavValue},
        {QStringLiteral("DB 동기화"), &m_adminHealthDbValue},
        {QStringLiteral("네트워크 품질"), &m_adminHealthNetValue}
    };

    for (const auto &row : rows) {
        auto *rowFrame = panelFrame(QStringLiteral("healthRow"));
        auto *rowLayout = new QVBoxLayout(rowFrame);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);

        auto *labelRow = new QHBoxLayout;
        labelRow->setContentsMargins(0, 0, 0, 0);
        labelRow->addWidget(makeLabel(row.first, QStringLiteral("healthLabel")));
        labelRow->addStretch();
        *row.second = makeLabel(QStringLiteral("--"), QStringLiteral("healthValue"));
        labelRow->addWidget(*row.second);
        rowLayout->addLayout(labelRow);

        auto *bar = panelFrame(QStringLiteral("healthBar"));
        bar->setProperty("kind", row.first);
        bar->setFixedHeight(8);
        rowLayout->addWidget(bar);
        layout->addWidget(rowFrame);
    }

    layout->addStretch();
    return panel;
}

QWidget *DashboardCentralWidget::buildAdminEventsPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("최근 운영 이벤트"), QStringLiteral("panelTitle")));

    m_adminEventsTable = new QTableWidget(2, 3, panel);
    m_adminEventsTable->setObjectName(QStringLiteral("eventsOverviewTable"));
    m_adminEventsTable->setHorizontalHeaderLabels({
        QStringLiteral("시각"),
        QStringLiteral("이벤트"),
        QStringLiteral("상태")
    });
    m_adminEventsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_adminEventsTable->verticalHeader()->setVisible(false);
    m_adminEventsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_adminEventsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_adminEventsTable->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_adminEventsTable);
    return panel;
}

QWidget *DashboardCentralWidget::buildUserView()
{
    auto *shell = panelFrame(QStringLiteral("pageShell"));
    auto *layout = new QVBoxLayout(shell);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(18);

    auto *topBrandRow = new QHBoxLayout;
    auto *brandA = makeBrandLogoLabel(shell, QSize(40, 40));
    auto *brandTitle = makeLabel(QStringLiteral("AURIX"), QStringLiteral("brandTitle"));
    auto *brandSubtitle = makeLabel(QStringLiteral("MEDIRUN CONTROL"), QStringLiteral("brandSubtitle"));
    topBrandRow->addWidget(brandA);
    auto *brandTextCol = new QVBoxLayout;
    brandTextCol->setSpacing(0);
    brandTextCol->addWidget(brandTitle);
    brandTextCol->addWidget(brandSubtitle);
    topBrandRow->addLayout(brandTextCol);
    topBrandRow->addStretch();
    m_userOnlineBadge = makeLabel(QStringLiteral("MR-02 ONLINE"), QStringLiteral("statusBadgeGreen"));
    topBrandRow->addWidget(m_userOnlineBadge);
    layout->addLayout(topBrandRow);

    auto *divider = panelFrame(QStringLiteral("sectionDivider"));
    divider->setFixedHeight(1);
    layout->addWidget(divider);

    auto *headerRow = new QHBoxLayout;
    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(6);
    titleCol->addWidget(makeLabel(QStringLiteral("내 로봇 관리"), QStringLiteral("pageTitle")));
    m_userDescriptionLabel = makeLabel(QStringLiteral("할당 로봇 MR-02의 상태와 임무를 안전하게 관리합니다."), QStringLiteral("pageSubtitle"));
    titleCol->addWidget(m_userDescriptionLabel);
    headerRow->addLayout(titleCol);
    headerRow->addStretch();
    auto *exitButton = new QPushButton(QStringLiteral("대시보드 종료"));
    exitButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(exitButton, &QPushButton::clicked, this, &DashboardCentralWidget::exitRequested);
    headerRow->addWidget(exitButton);
    auto *logoutButton = new QPushButton(QStringLiteral("로그아웃"));
    logoutButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(logoutButton, &QPushButton::clicked, this, &DashboardCentralWidget::logoutRequested);
    headerRow->addWidget(logoutButton);
    auto *estop = new QPushButton(QStringLiteral("■  긴급 정지"));
    estop->setObjectName(QStringLiteral("userDangerButton"));
    connect(estop, &QPushButton::clicked, this, &DashboardCentralWidget::estopRequested);
    headerRow->addWidget(estop);
    layout->addLayout(headerRow);

    auto *metricRow = new QGridLayout;
    metricRow->setHorizontalSpacing(12);
    metricRow->addWidget(buildUserMetricCard(QStringLiteral("배터리"), m_userBatteryValue, QStringLiteral("green"), QStringLiteral("약 4시간 12분 사용 가능")), 0, 0);
    metricRow->addWidget(buildUserMetricCard(QStringLiteral("현재 상태"), m_userStateValue, QStringLiteral("cyan"), QStringLiteral("충전 스테이션 A 대기")), 0, 1);
    metricRow->addWidget(buildUserMetricCard(QStringLiteral("오늘 미션"), m_userMissionCountValue, QStringLiteral("blue"), QStringLiteral("완료 5 · 실패 0")), 0, 2);
    layout->addLayout(metricRow);

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(14);
    bodyRow->addWidget(buildUserRobotPanel(), 7);
    bodyRow->addWidget(buildUserMissionPanel(), 5);
    layout->addLayout(bodyRow, 1);
    return shell;
}

QWidget *DashboardCentralWidget::buildUserMetricCard(const QString &title, QLabel *&valueLabel, const QString &accentName, const QString &caption)
{
    auto *card = panelFrame(QStringLiteral("metricCard"));
    card->setProperty("accent", accentName);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);
    auto *topRow = new QHBoxLayout;
    topRow->addWidget(makeLabel(title, QStringLiteral("metricTitle")));
    topRow->addStretch();
    auto *dot = makeLabel(QStringLiteral("•"), QStringLiteral("metricDot"));
    dot->setProperty("accent", accentName);
    topRow->addWidget(dot);
    layout->addLayout(topRow);
    valueLabel = makeLabel(QStringLiteral("--"), QStringLiteral("metricValue"));
    layout->addWidget(valueLabel);
    layout->addWidget(makeLabel(caption, QStringLiteral("metricCaption")));
    return card;
}

QWidget *DashboardCentralWidget::buildUserRobotPanel()
{
    auto *panel = panelFrame(QStringLiteral("userRobotPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("LIVE ROBOT"), QStringLiteral("sectionKicker")));
    m_userRobotIdLabel = makeLabel(QStringLiteral("MR-02"), QStringLiteral("userRobotTitle"));
    layout->addWidget(m_userRobotIdLabel);
    m_userRobotSubtitleLabel = makeLabel(QStringLiteral("AURIX MEDIRUN RESPONSE UNIT"), QStringLiteral("userRobotSubtitle"));
    layout->addWidget(m_userRobotSubtitleLabel);

    auto *radarWrap = panelFrame(QStringLiteral("robotRadarPlaceholder"));
    auto *radarLayout = new QVBoxLayout(radarWrap);
    radarLayout->setContentsMargins(12, 12, 12, 12);
    auto *robotVisual = makeRobotVisualLabel(radarWrap, QSize(360, 260));
    radarLayout->addStretch();
    radarLayout->addWidget(robotVisual, 0, Qt::AlignCenter);
    radarLayout->addStretch();
    layout->addWidget(radarWrap, 1);

    m_userRobotStatusLabel = makeLabel(QStringLiteral("LIDAR 정상 · IMU 정상 · NAV2 준비"), QStringLiteral("userStatusTags"));
    layout->addWidget(m_userRobotStatusLabel);
    m_userHeartbeatLabel = makeLabel(QStringLiteral("Heartbeat 1초 전 · 24ms"), QStringLiteral("pageSubtitle"));
    layout->addWidget(m_userHeartbeatLabel);
    return panel;
}

QWidget *DashboardCentralWidget::buildUserMissionPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("빠른 미션"), QStringLiteral("panelTitle")));
    layout->addWidget(makeLabel(QStringLiteral("호출 위치를 선택하면 로봇이 경로 안전성을 확인한 후 출동합니다."), QStringLiteral("metricCaption")));

    m_waypointOneButton = new QPushButton(QStringLiteral("W-01   중앙 대합실                         →"));
    m_waypointTwoButton = new QPushButton(QStringLiteral("W-03   동측 승강장                         →"));
    m_waypointThreeButton = new QPushButton(QStringLiteral("W-07   서측 출입구                         →"));
    const QList<QPushButton *> missionButtons = {m_waypointOneButton, m_waypointTwoButton, m_waypointThreeButton};
    for (QPushButton *button : missionButtons) {
        button->setObjectName(QStringLiteral("missionActionButton"));
        layout->addWidget(button);
    }

    auto *missionDivider = panelFrame(QStringLiteral("sectionDivider"));
    missionDivider->setFixedHeight(1);
    layout->addWidget(missionDivider);
    layout->addWidget(makeLabel(QStringLiteral("최근 미션"), QStringLiteral("panelTitle")));
    m_userMissionLogLabel = makeLabel(QStringLiteral("--"), QStringLiteral("missionLogLabel"));
    layout->addWidget(m_userMissionLogLabel);
    layout->addStretch();
    return panel;
}

void DashboardCentralWidget::switchAdminSection(int index)
{
    if (m_adminStack == nullptr || index < 0 || index >= m_adminNavButtons.size()) {
        return;
    }

    m_adminStack->setCurrentIndex(index);

    for (int buttonIndex = 0; buttonIndex < m_adminNavButtons.size(); ++buttonIndex) {
        QPushButton *button = m_adminNavButtons.at(buttonIndex);
        button->setObjectName(buttonIndex == index
                                  ? QStringLiteral("sidebarButtonActive")
                                  : QStringLiteral("sidebarButton"));
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void DashboardCentralWidget::refreshUserTable()
{
    if (m_userManagementTable == nullptr) {
        return;
    }

    const QVector<AuthUserRecord> users = m_authService.users();
    m_userManagementTable->setRowCount(users.size());
    for (int row = 0; row < users.size(); ++row) {
        const AuthUserRecord &user = users.at(row);
        m_userManagementTable->setItem(row, 0, new QTableWidgetItem(user.loginId));
        m_userManagementTable->setItem(row, 1, new QTableWidgetItem(roleDisplayName(user.role)));
    }
}

void DashboardCentralWidget::handleCreateUser()
{
    if (m_userCreateEmailEdit == nullptr || m_userCreatePasswordEdit == nullptr || m_userCreateRoleCombo == nullptr) {
        return;
    }

    QString errorMessage;
    const UserRole role = static_cast<UserRole>(m_userCreateRoleCombo->currentData().toInt());
    const bool ok = m_authService.createUser(
        m_userCreateEmailEdit->text(),
        m_userCreatePasswordEdit->text(),
        role,
        errorMessage);

    if (!ok) {
        if (m_userCreateStatusLabel != nullptr) {
            m_userCreateStatusLabel->setText(errorMessage);
        }
        return;
    }

    if (m_userCreateStatusLabel != nullptr) {
        m_userCreateStatusLabel->setText(
            QStringLiteral("%1 계정이 생성되었습니다. 이제 해당 계정으로 로그인할 수 있습니다.")
                .arg(m_userCreateEmailEdit->text().trimmed()));
    }

    m_userCreateEmailEdit->clear();
    m_userCreatePasswordEdit->clear();
    refreshUserTable();
}

void DashboardCentralWidget::setBadgeStyle(QWidget *widget, const QString &kind)
{
    widget->setProperty("badgeKind", kind);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void DashboardCentralWidget::updateAdminSnapshot(const TelemetrySnapshot &snapshot)
{
    if (m_adminTotalRobotsValue == nullptr) {
        return;
    }

    m_adminTotalRobotsValue->setText(QStringLiteral("03"));
    m_adminActiveMissionValue->setText(snapshot.missionState == QStringLiteral("IDLE") ? QStringLiteral("00") : QStringLiteral("02"));
    m_adminTodayCallsValue->setText(QStringLiteral("17"));
    m_adminSafetyEventsValue->setText(snapshot.stopReason == QStringLiteral("--") ? QStringLiteral("00") : QStringLiteral("01"));

    const QList<QStringList> rows = {
        {QStringLiteral("MR-01 · 응급 키트 운반"), statePill(QStringLiteral("MOVING"), QStringLiteral("cyan")), QStringLiteral("86%"), QStringLiteral("B1 중앙홀 → W-03")},
        {QStringLiteral("MR-02 · 대기 중"), statePill(QStringLiteral("READY"), QStringLiteral("green")), QStringLiteral("94%"), QStringLiteral("충전 스테이션 A")},
        {QStringLiteral("MR-03 · 복귀 중"), statePill(QStringLiteral("MOVING"), QStringLiteral("cyan")), QStringLiteral("61%"), QStringLiteral("W-07 → 충전 스테이션")}
    };
    m_adminRobotTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        for (int col = 0; col < rows.at(row).size(); ++col) {
            auto *item = new QTableWidgetItem(rows.at(row).at(col));
            m_adminRobotTable->setItem(row, col, item);
        }
    }

    m_adminHealthRosValue->setText(QStringLiteral("96%"));
    m_adminHealthNavValue->setText(QStringLiteral("94%"));
    m_adminHealthDbValue->setText(QStringLiteral("100%"));
    m_adminHealthNetValue->setText(QStringLiteral("87%"));

    const QVector<SystemEventItem> events = snapshot.recentEvents;
    m_adminEventsTable->setRowCount(qMin(2, events.size()));
    for (int row = 0; row < m_adminEventsTable->rowCount(); ++row) {
        const auto &event = events.at(row);
        m_adminEventsTable->setItem(row, 0, new QTableWidgetItem(event.occurredAt.toString(QStringLiteral("hh:mm"))));
        m_adminEventsTable->setItem(row, 1, new QTableWidgetItem(event.message));
        m_adminEventsTable->setItem(row, 2, new QTableWidgetItem(event.synced ? QStringLiteral("완료") : QStringLiteral("처리 중")));
    }
}

void DashboardCentralWidget::updateUserSnapshot(const TelemetrySnapshot &snapshot)
{
    if (m_userBatteryValue == nullptr) {
        return;
    }

    m_userBatteryValue->setText(snapshot.batteryPercentText.isEmpty() ? QStringLiteral("94%") : snapshot.batteryPercentText);
    m_userStateValue->setText(snapshot.missionState == QStringLiteral("IDLE") ? QStringLiteral("READY") : snapshot.missionState);
    m_userMissionCountValue->setText(QStringLiteral("05"));
    m_userRobotIdLabel->setText(snapshot.robotName.isEmpty() ? QStringLiteral("MR-02") : snapshot.robotName);
    m_userRobotSubtitleLabel->setText(QStringLiteral("%1 RESPONSE UNIT").arg(m_session.username.toUpper()));
    m_userHeartbeatLabel->setText(QStringLiteral("%1 · %2").arg(snapshot.rosText, snapshot.connectionDelayText));
    m_userRobotStatusLabel->setText(QStringLiteral("ROS2 %1 · NAV2 %2 · DB %3").arg(snapshot.rosText, snapshot.mode, snapshot.dbText));
    m_userMissionLogLabel->setText(formatRecentMissionLines(snapshot.recentEvents));
    m_userDescriptionLabel->setText(QStringLiteral("%1의 담당 로봇 %2 상태와 임무를 안전하게 관리합니다.").arg(m_session.username, snapshot.robotName));
    m_userOnlineBadge->setText(QStringLiteral("%1 ONLINE").arg(snapshot.robotName));
}

QString DashboardCentralWidget::statePill(const QString &text, const QString &kind) const
{
    if (kind == QStringLiteral("green")) {
        return QStringLiteral("[READY] %1").arg(text);
    }
    if (kind == QStringLiteral("amber")) {
        return QStringLiteral("[WARN] %1").arg(text);
    }
    return QStringLiteral("[LIVE] %1").arg(text);
}

QString DashboardCentralWidget::formatRecentMissionLines(const QVector<SystemEventItem> &events) const
{
    QStringList lines;
    for (const auto &event : events) {
        lines << QStringLiteral("%1   %2   %3")
                     .arg(event.occurredAt.toString(QStringLiteral("hh:mm")))
                     .arg(event.message)
                     .arg(event.synced ? QStringLiteral("완료") : QStringLiteral("처리 중"));
    }
    if (lines.isEmpty()) {
        lines << QStringLiteral("08:54   W-03 응급 키트 운반   완료")
              << QStringLiteral("08:12   W-01 현장 출동        완료");
    }
    return lines.join(QLatin1Char('\n'));
}
