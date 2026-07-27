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
#include <QSlider>
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

QWidget *makeInfoRow(const QString &labelText, QLabel *valueLabel, QWidget *parent = nullptr)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *label = makeLabel(labelText, QStringLiteral("metricTitle"));
    label->setMinimumWidth(112);
    layout->addWidget(label, 0, Qt::AlignTop);
    layout->addWidget(valueLabel, 1);
    return row;
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
    , m_userStack(nullptr)
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
    , m_userAssignedRobotValue(nullptr)
    , m_userBatteryValue(nullptr)
    , m_userStateValue(nullptr)
    , m_userMissionCountValue(nullptr)
    , m_userConnectionValue(nullptr)
    , m_userRobotIdLabel(nullptr)
    , m_userRobotSubtitleLabel(nullptr)
    , m_userRobotStatusLabel(nullptr)
    , m_userHeartbeatLabel(nullptr)
    , m_userHealthTagsLabel(nullptr)
    , m_userMissionLogLabel(nullptr)
    , m_userRobotPageTitleLabel(nullptr)
    , m_userRobotDetailLabel(nullptr)
    , m_userRobotDeviceTable(nullptr)
    , m_userSensorTable(nullptr)
    , m_userCameraStatusLabel(nullptr)
    , m_userCameraTopicLabel(nullptr)
    , m_userCameraEventsLabel(nullptr)
    , m_userMapMissionLabel(nullptr)
    , m_userMapPositionLabel(nullptr)
    , m_userMapDistanceLabel(nullptr)
    , m_userSshStatusLabel(nullptr)
    , m_userSshNetworkLabel(nullptr)
    , m_userSshDbLabel(nullptr)
    , m_userLogTable(nullptr)
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
    m_userCreateRoleCombo->addItem(QStringLiteral("USER"), static_cast<int>(UserRole::User));
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

    layout->addWidget(buildUserPageHeader());
    layout->addWidget(buildUserNavBar());

    m_userStack = new QStackedWidget(shell);
    m_userStack->setObjectName(QStringLiteral("userPageStack"));
    m_userStack->addWidget(buildUserDashboardPage());
    m_userStack->addWidget(buildUserRobotPage());
    m_userStack->addWidget(buildUserSensorPage());
    m_userStack->addWidget(buildUserCameraPage());
    m_userStack->addWidget(buildUserMapPage());
    m_userStack->addWidget(buildUserSshAgentPage());
    m_userStack->addWidget(buildUserLogReplayPage());
    layout->addWidget(m_userStack, 1);

    switchUserSection(0);
    return shell;
}

QWidget *DashboardCentralWidget::buildUserPageHeader()
{
    auto *headerPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *headerLayout = new QHBoxLayout(headerPanel);
    headerLayout->setContentsMargins(20, 18, 20, 18);
    headerLayout->setSpacing(10);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(6);
    titleCol->addWidget(makeLabel(QStringLiteral("내 로봇 관리"), QStringLiteral("pageTitle")));
    m_userDescriptionLabel = makeLabel(QStringLiteral("할당 로봇 MR-02의 상태와 임무를 안전하게 관리합니다."), QStringLiteral("pageSubtitle"));
    titleCol->addWidget(m_userDescriptionLabel);
    headerLayout->addLayout(titleCol);
    headerLayout->addStretch();

    auto *exitButton = new QPushButton(QStringLiteral("대시보드 종료"));
    exitButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(exitButton, &QPushButton::clicked, this, &DashboardCentralWidget::exitRequested);
    headerLayout->addWidget(exitButton);

    auto *logoutButton = new QPushButton(QStringLiteral("로그아웃"));
    logoutButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(logoutButton, &QPushButton::clicked, this, &DashboardCentralWidget::logoutRequested);
    headerLayout->addWidget(logoutButton);

    auto *estop = new QPushButton(QStringLiteral("■  긴급 정지"));
    estop->setObjectName(QStringLiteral("userDangerButton"));
    connect(estop, &QPushButton::clicked, this, &DashboardCentralWidget::estopRequested);
    headerLayout->addWidget(estop);
    return headerPanel;
}

QWidget *DashboardCentralWidget::buildUserNavBar()
{
    auto *navFrame = panelFrame(QStringLiteral("userNavFrame"));
    auto *layout = new QGridLayout(navFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(0);
    layout->setVerticalSpacing(0);

    const QStringList items = {
        QStringLiteral("Dashboard"),
        QStringLiteral("Robot"),
        QStringLiteral("Sensor"),
        QStringLiteral("Camera"),
        QStringLiteral("Map"),
        QStringLiteral("SSH/Agent"),
        QStringLiteral("Log/Replay")
    };

    for (int index = 0; index < items.size(); ++index) {
        auto *button = new QPushButton(items.at(index), navFrame);
        button->setObjectName(QStringLiteral("userNavButton"));
        button->setCheckable(false);
        layout->addWidget(button, 0, index);
        m_userNavButtons.append(button);
        connect(button, &QPushButton::clicked, this, [this, index]() { switchUserSection(index); });
    }

    return navFrame;
}

QWidget *DashboardCentralWidget::buildUserDashboardPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *metricRow = new QGridLayout;
    metricRow->setHorizontalSpacing(12);
    metricRow->setVerticalSpacing(12);
    metricRow->addWidget(buildUserMetricCard(QStringLiteral("할당 로봇"), m_userAssignedRobotValue, QStringLiteral("amber"), QStringLiteral("현재 세션에 연결된 운용 로봇")), 0, 0);
    metricRow->addWidget(buildUserMetricCard(QStringLiteral("배터리"), m_userBatteryValue, QStringLiteral("green"), QStringLiteral("예상 가동 시간 기준")), 0, 1);
    metricRow->addWidget(buildUserMetricCard(QStringLiteral("미션 상태"), m_userStateValue, QStringLiteral("cyan"), QStringLiteral("현재 로봇 작업 상태")), 0, 2);
    metricRow->addWidget(buildUserMetricCard(QStringLiteral("연결 상태"), m_userConnectionValue, QStringLiteral("blue"), QStringLiteral("ROS2 및 네트워크 연결 품질")), 0, 3);
    layout->addLayout(metricRow);

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(20);
    bodyRow->addWidget(buildUserRobotPanel(), 1);
    bodyRow->addWidget(buildUserMissionPanel(), 1);
    layout->addLayout(bodyRow, 1);
    return page;
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
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);
    layout->addWidget(makeLabel(QStringLiteral("Platform Health"), QStringLiteral("panelTitle")));

    m_userRobotIdLabel = makeLabel(QStringLiteral("MR-02"), QStringLiteral("metricValue"));
    layout->addWidget(makeInfoRow(QStringLiteral("Robot"), m_userRobotIdLabel, panel));

    m_userRobotSubtitleLabel = makeLabel(QStringLiteral("현재 로그인 세션에 연결됨"), QStringLiteral("pageSubtitle"));
    layout->addWidget(makeInfoRow(QStringLiteral("Session"), m_userRobotSubtitleLabel, panel));

    m_userRobotStatusLabel = makeLabel(QStringLiteral("LIDAR 정상 · IMU 정상 · NAV2 준비"), QStringLiteral("pageSubtitle"));
    layout->addWidget(makeInfoRow(QStringLiteral("Status"), m_userRobotStatusLabel, panel));

    m_userHeartbeatLabel = makeLabel(QStringLiteral("Heartbeat 1초 전 · 24ms"), QStringLiteral("pageSubtitle"));
    layout->addWidget(makeInfoRow(QStringLiteral("Heartbeat"), m_userHeartbeatLabel, panel));

    m_userHealthTagsLabel = makeLabel(QStringLiteral("배정 상태 정상 · 경로 계획 가능 · 충전 여유 충분"), QStringLiteral("pageSubtitle"));
    layout->addWidget(makeInfoRow(QStringLiteral("Safety"), m_userHealthTagsLabel, panel));

    auto *radarWrap = panelFrame(QStringLiteral("robotRadarPlaceholder"));
    auto *radarLayout = new QVBoxLayout(radarWrap);
    radarLayout->setContentsMargins(12, 12, 12, 12);
    auto *robotVisual = makeRobotVisualLabel(radarWrap, QSize(320, 220));
    radarLayout->addStretch();
    radarLayout->addWidget(robotVisual, 0, Qt::AlignCenter);
    radarLayout->addStretch();
    layout->addWidget(radarWrap, 1);
    return panel;
}

QWidget *DashboardCentralWidget::buildUserMissionPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("Recent Events"), QStringLiteral("panelTitle")));
    layout->addWidget(makeLabel(QStringLiteral("최근 로봇 상태 이벤트와 사용자 미션 요청을 시간 순으로 확인합니다."), QStringLiteral("metricCaption")));

    m_userMissionLogLabel = makeLabel(QStringLiteral("--"), QStringLiteral("missionLogLabel"));
    layout->addWidget(m_userMissionLogLabel);

    m_waypointOneButton = new QPushButton(QStringLiteral("W-01   중앙 대합실                         →"));
    m_waypointTwoButton = new QPushButton(QStringLiteral("W-03   동측 승강장                         →"));
    m_waypointThreeButton = new QPushButton(QStringLiteral("W-07   서측 출입구                         →"));

    auto *missionDivider = panelFrame(QStringLiteral("sectionDivider"));
    missionDivider->setFixedHeight(1);
    layout->addWidget(missionDivider);

    layout->addWidget(makeLabel(QStringLiteral("Quick Mission"), QStringLiteral("panelTitle")));
    layout->addWidget(makeLabel(QStringLiteral("호출 위치를 선택하면 로봇이 경로 안전성을 확인한 뒤 출동합니다."), QStringLiteral("metricCaption")));
    const QList<QPushButton *> missionButtons = {m_waypointOneButton, m_waypointTwoButton, m_waypointThreeButton};
    for (QPushButton *button : missionButtons) {
        button->setObjectName(QStringLiteral("missionActionButton"));
        layout->addWidget(button);
    }

    layout->addStretch();
    return panel;
}

QWidget *DashboardCentralWidget::buildUserRobotPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *headerPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *headerLayout = new QVBoxLayout(headerPanel);
    headerLayout->setContentsMargins(20, 18, 20, 18);
    headerLayout->setSpacing(6);
    m_userRobotPageTitleLabel = makeLabel(QStringLiteral("Robot"), QStringLiteral("pageTitle"));
    headerLayout->addWidget(m_userRobotPageTitleLabel);
    m_userRobotDetailLabel = makeLabel(QStringLiteral("현재 할당된 로봇 상태와 장치별 동작 상태를 확인합니다."), QStringLiteral("pageSubtitle"));
    headerLayout->addWidget(m_userRobotDetailLabel);
    layout->addWidget(headerPanel);

    auto *body = new QHBoxLayout;
    body->setSpacing(18);
    body->addWidget(buildUserRobotPanel(), 1);

    auto *devicePanel = panelFrame(QStringLiteral("contentPanel"));
    auto *deviceLayout = new QVBoxLayout(devicePanel);
    deviceLayout->setContentsMargins(16, 16, 16, 16);
    deviceLayout->setSpacing(12);
    deviceLayout->addWidget(makeLabel(QStringLiteral("장치 상태"), QStringLiteral("panelTitle")));
    m_userRobotDeviceTable = new QTableWidget(0, 2, devicePanel);
    m_userRobotDeviceTable->setHorizontalHeaderLabels({QStringLiteral("장치"), QStringLiteral("상태")});
    m_userRobotDeviceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_userRobotDeviceTable->verticalHeader()->setVisible(false);
    m_userRobotDeviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userRobotDeviceTable->setSelectionMode(QAbstractItemView::NoSelection);
    deviceLayout->addWidget(m_userRobotDeviceTable, 1);
    body->addWidget(devicePanel, 1);

    layout->addLayout(body, 1);
    return page;
}

QWidget *DashboardCentralWidget::buildUserSensorPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *headerPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *headerLayout = new QVBoxLayout(headerPanel);
    headerLayout->setContentsMargins(20, 18, 20, 18);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(makeLabel(QStringLiteral("Sensor"), QStringLiteral("pageTitle")));
    headerLayout->addWidget(makeLabel(QStringLiteral("센서 입력과 연결 지연 상태를 확인합니다."), QStringLiteral("pageSubtitle")));
    layout->addWidget(headerPanel);

    auto *sensorPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *sensorLayout = new QVBoxLayout(sensorPanel);
    sensorLayout->setContentsMargins(16, 16, 16, 16);
    sensorLayout->setSpacing(12);
    sensorLayout->addWidget(makeLabel(QStringLiteral("센서 상세"), QStringLiteral("panelTitle")));
    m_userSensorTable = new QTableWidget(0, 3, sensorPanel);
    m_userSensorTable->setHorizontalHeaderLabels({QStringLiteral("항목"), QStringLiteral("값"), QStringLiteral("상태")});
    m_userSensorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_userSensorTable->verticalHeader()->setVisible(false);
    m_userSensorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userSensorTable->setSelectionMode(QAbstractItemView::NoSelection);
    sensorLayout->addWidget(m_userSensorTable, 1);
    layout->addWidget(sensorPanel, 1);
    return page;
}

QWidget *DashboardCentralWidget::buildUserCameraPage()
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *previewPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewLayout->setSpacing(12);
    previewLayout->addWidget(makeLabel(QStringLiteral("Camera"), QStringLiteral("pageTitle")));

    auto *cameraSurface = panelFrame(QStringLiteral("robotRadarPlaceholder"));
    cameraSurface->setMinimumHeight(360);
    auto *cameraSurfaceLayout = new QVBoxLayout(cameraSurface);
    cameraSurfaceLayout->setContentsMargins(18, 18, 18, 18);
    cameraSurfaceLayout->setSpacing(12);
    cameraSurfaceLayout->addWidget(makeLabel(QStringLiteral("Front RGB Camera"), QStringLiteral("panelTitle")));
    auto *cameraCanvas = makeLabel(QStringLiteral("LIVE PREVIEW\n1280 x 720"), QStringLiteral("previewHeroTitle"));
    cameraCanvas->setAlignment(Qt::AlignCenter);
    cameraSurfaceLayout->addWidget(cameraCanvas, 1);

    auto *slider = new QSlider(Qt::Horizontal, cameraSurface);
    slider->setRange(0, 100);
    slider->setValue(18);
    cameraSurfaceLayout->addWidget(slider);

    auto *controlRow = new QHBoxLayout;
    auto *captureButton = new QPushButton(QStringLiteral("캡처"));
    captureButton->setObjectName(QStringLiteral("inlineGhostButton"));
    auto *recordButton = new QPushButton(QStringLiteral("기록"));
    recordButton->setObjectName(QStringLiteral("primaryCtaButton"));
    controlRow->addWidget(captureButton);
    controlRow->addWidget(recordButton);
    controlRow->addStretch();
    cameraSurfaceLayout->addLayout(controlRow);
    previewLayout->addWidget(cameraSurface, 1);
    layout->addWidget(previewPanel, 2);

    auto *infoPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(16, 16, 16, 16);
    infoLayout->setSpacing(12);
    infoLayout->addWidget(makeLabel(QStringLiteral("카메라 상태"), QStringLiteral("panelTitle")));
    m_userCameraStatusLabel = makeLabel(QStringLiteral("LIVE"), QStringLiteral("metricValue"));
    infoLayout->addWidget(makeInfoRow(QStringLiteral("Stream"), m_userCameraStatusLabel, infoPanel));
    m_userCameraTopicLabel = makeLabel(QStringLiteral("/medirun/camera/front"), QStringLiteral("pageSubtitle"));
    infoLayout->addWidget(makeInfoRow(QStringLiteral("Topic"), m_userCameraTopicLabel, infoPanel));
    m_userCameraEventsLabel = makeLabel(QStringLiteral("최근 이벤트 없음"), QStringLiteral("missionLogLabel"));
    infoLayout->addWidget(makeLabel(QStringLiteral("최근 감지"), QStringLiteral("panelTitle")));
    infoLayout->addWidget(m_userCameraEventsLabel);

    auto *detectionTable = new QTableWidget(4, 3, infoPanel);
    detectionTable->setHorizontalHeaderLabels({QStringLiteral("Label"), QStringLiteral("Confidence"), QStringLiteral("Status")});
    detectionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    detectionTable->verticalHeader()->setVisible(false);
    detectionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detectionTable->setSelectionMode(QAbstractItemView::NoSelection);
    const QList<QStringList> detections = {
        {QStringLiteral("person"), QStringLiteral("0.96"), QStringLiteral("TRACK")},
        {QStringLiteral("marker"), QStringLiteral("0.82"), QStringLiteral("LOCK")},
        {QStringLiteral("forklift"), QStringLiteral("0.71"), QStringLiteral("WARN")},
        {QStringLiteral("obstacle"), QStringLiteral("0.64"), QStringLiteral("SCAN")}
    };
    for (int row = 0; row < detections.size(); ++row) {
        for (int col = 0; col < detections.at(row).size(); ++col) {
            detectionTable->setItem(row, col, new QTableWidgetItem(detections.at(row).at(col)));
        }
    }
    infoLayout->addWidget(makeLabel(QStringLiteral("객체 감지"), QStringLiteral("panelTitle")));
    infoLayout->addWidget(detectionTable, 1);
    infoLayout->addStretch();
    layout->addWidget(infoPanel, 1);
    return page;
}

QWidget *DashboardCentralWidget::buildUserMapPage()
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *mapPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *mapLayout = new QVBoxLayout(mapPanel);
    mapLayout->setContentsMargins(16, 16, 16, 16);
    mapLayout->setSpacing(12);
    mapLayout->addWidget(makeLabel(QStringLiteral("Map"), QStringLiteral("pageTitle")));

    auto *mapSurface = panelFrame(QStringLiteral("robotRadarPlaceholder"));
    mapSurface->setMinimumHeight(420);
    auto *mapSurfaceLayout = new QVBoxLayout(mapSurface);
    mapSurfaceLayout->setContentsMargins(18, 18, 18, 18);
    mapSurfaceLayout->setSpacing(16);

    auto *overlayRow = new QHBoxLayout;
    auto *cameraMini = panelFrame(QStringLiteral("contentPanel"));
    cameraMini->setMaximumWidth(220);
    auto *cameraMiniLayout = new QVBoxLayout(cameraMini);
    cameraMiniLayout->setContentsMargins(12, 12, 12, 12);
    cameraMiniLayout->addWidget(makeLabel(QStringLiteral("Front Camera"), QStringLiteral("panelTitle")));
    auto *miniText = makeLabel(QStringLiteral("LIVE VIEW\nForward Lane"), QStringLiteral("metricValue"));
    miniText->setAlignment(Qt::AlignCenter);
    cameraMiniLayout->addWidget(miniText);
    overlayRow->addWidget(cameraMini, 0, Qt::AlignTop);
    overlayRow->addStretch();
    mapSurfaceLayout->addLayout(overlayRow);
    mapSurfaceLayout->addStretch();
    auto *mapState = makeLabel(QStringLiteral("MISSION MAP\nQt Dashboard -> ROS2 Robot"), QStringLiteral("previewHeroTitle"));
    mapState->setAlignment(Qt::AlignCenter);
    mapSurfaceLayout->addWidget(mapState);
    mapSurfaceLayout->addWidget(makeLabel(QStringLiteral("실시간 위치와 경로 상태가 이 영역에 표시됩니다."), QStringLiteral("pageSubtitle")), 0, Qt::AlignCenter);
    mapSurfaceLayout->addStretch();
    mapLayout->addWidget(mapSurface, 1);
    layout->addWidget(mapPanel, 2);

    auto *summaryPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(16, 16, 16, 16);
    summaryLayout->setSpacing(12);
    summaryLayout->addWidget(makeLabel(QStringLiteral("미션 요약"), QStringLiteral("panelTitle")));
    m_userMapMissionLabel = makeLabel(QStringLiteral("--"), QStringLiteral("pageSubtitle"));
    summaryLayout->addWidget(makeInfoRow(QStringLiteral("Mission"), m_userMapMissionLabel, summaryPanel));
    m_userMapPositionLabel = makeLabel(QStringLiteral("--"), QStringLiteral("pageSubtitle"));
    summaryLayout->addWidget(makeInfoRow(QStringLiteral("Position"), m_userMapPositionLabel, summaryPanel));
    m_userMapDistanceLabel = makeLabel(QStringLiteral("--"), QStringLiteral("pageSubtitle"));
    summaryLayout->addWidget(makeInfoRow(QStringLiteral("Distance"), m_userMapDistanceLabel, summaryPanel));
    summaryLayout->addWidget(makeLabel(QStringLiteral("빠른 이동"), QStringLiteral("panelTitle")));
    auto *buttons = new QVBoxLayout;
    auto *mapWp1 = new QPushButton(QStringLiteral("W-01   중앙 대합실                         →"));
    auto *mapWp2 = new QPushButton(QStringLiteral("W-03   동측 승강장                         →"));
    auto *mapWp3 = new QPushButton(QStringLiteral("W-07   서측 출입구                         →"));
    for (QPushButton *button : {mapWp1, mapWp2, mapWp3}) {
        button->setObjectName(QStringLiteral("missionActionButton"));
        buttons->addWidget(button);
    }
    summaryLayout->addLayout(buttons);
    auto *actionRow = new QHBoxLayout;
    auto *returnButton = new QPushButton(QStringLiteral("복귀"));
    returnButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(returnButton, &QPushButton::clicked, this, &DashboardCentralWidget::returnHomeRequested);
    auto *stopButton = new QPushButton(QStringLiteral("정지"));
    stopButton->setObjectName(QStringLiteral("userDangerButton"));
    connect(stopButton, &QPushButton::clicked, this, &DashboardCentralWidget::estopRequested);
    actionRow->addWidget(returnButton);
    actionRow->addWidget(stopButton);
    summaryLayout->addLayout(actionRow);
    summaryLayout->addStretch();
    layout->addWidget(summaryPanel, 1);
    return page;
}

QWidget *DashboardCentralWidget::buildUserSshAgentPage()
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *leftPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 16, 16, 16);
    leftLayout->setSpacing(12);
    leftLayout->addWidget(makeLabel(QStringLiteral("SSH/Agent"), QStringLiteral("pageTitle")));
    auto *agentGuide = panelFrame(QStringLiteral("robotRadarPlaceholder"));
    agentGuide->setMinimumHeight(320);
    auto *agentGuideLayout = new QVBoxLayout(agentGuide);
    agentGuideLayout->setContentsMargins(18, 18, 18, 18);
    agentGuideLayout->setSpacing(12);
    agentGuideLayout->addWidget(makeLabel(QStringLiteral("Bridge Summary"), QStringLiteral("panelTitle")));
    agentGuideLayout->addWidget(makeLabel(QStringLiteral("Qt 대시보드와 ROS2 에이전트, 로컬 DB, 네트워크 상태를 여기서 모니터링합니다."), QStringLiteral("missionLogLabel")));
    agentGuideLayout->addStretch();
    agentGuideLayout->addWidget(makeLabel(QStringLiteral("SSH 연결 요청, Agent 상태, 중앙 브리지 상태가 이 페이지에서 관리됩니다."), QStringLiteral("pageSubtitle")));
    leftLayout->addWidget(agentGuide, 1);
    layout->addWidget(leftPanel, 2);

    auto *rightPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 16, 16, 16);
    rightLayout->setSpacing(12);
    rightLayout->addWidget(makeLabel(QStringLiteral("연결 상태"), QStringLiteral("panelTitle")));
    m_userSshStatusLabel = makeLabel(QStringLiteral("--"), QStringLiteral("pageSubtitle"));
    rightLayout->addWidget(makeInfoRow(QStringLiteral("ROS"), m_userSshStatusLabel, rightPanel));
    m_userSshNetworkLabel = makeLabel(QStringLiteral("--"), QStringLiteral("pageSubtitle"));
    rightLayout->addWidget(makeInfoRow(QStringLiteral("Network"), m_userSshNetworkLabel, rightPanel));
    m_userSshDbLabel = makeLabel(QStringLiteral("--"), QStringLiteral("pageSubtitle"));
    rightLayout->addWidget(makeInfoRow(QStringLiteral("Database"), m_userSshDbLabel, rightPanel));
    auto *manualButton = new QPushButton(QStringLiteral("관리자 수동 제어 요청"));
    manualButton->setObjectName(QStringLiteral("missionActionButton"));
    connect(manualButton, &QPushButton::clicked, this, &DashboardCentralWidget::manualControlRequested);
    rightLayout->addWidget(manualButton);
    auto *returnButton = new QPushButton(QStringLiteral("홈 복귀 요청"));
    returnButton->setObjectName(QStringLiteral("missionActionButton"));
    connect(returnButton, &QPushButton::clicked, this, &DashboardCentralWidget::returnHomeRequested);
    rightLayout->addWidget(returnButton);
    rightLayout->addStretch();
    layout->addWidget(rightPanel, 1);
    return page;
}

QWidget *DashboardCentralWidget::buildUserLogReplayPage()
{
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    auto *leftPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(16, 16, 16, 16);
    leftLayout->setSpacing(12);
    leftLayout->addWidget(makeLabel(QStringLiteral("Log/Replay"), QStringLiteral("pageTitle")));
    auto *summaryPanel = panelFrame(QStringLiteral("robotRadarPlaceholder"));
    summaryPanel->setMinimumHeight(320);
    auto *summaryLayout = new QVBoxLayout(summaryPanel);
    summaryLayout->setContentsMargins(18, 18, 18, 18);
    summaryLayout->setSpacing(12);
    summaryLayout->addWidget(makeLabel(QStringLiteral("Replay Queue"), QStringLiteral("panelTitle")));
    summaryLayout->addWidget(makeLabel(QStringLiteral("최근 미션 이벤트를 시각 순으로 확인하고 운영 이력을 재검토합니다."), QStringLiteral("missionLogLabel")));
    auto *queueTable = new QTableWidget(3, 2, summaryPanel);
    queueTable->setHorizontalHeaderLabels({QStringLiteral("세션"), QStringLiteral("상태")});
    queueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    queueTable->verticalHeader()->setVisible(false);
    queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    queueTable->setSelectionMode(QAbstractItemView::NoSelection);
    const QList<QStringList> queueRows = {
        {QStringLiteral("MR-20260725-001"), QStringLiteral("완료")},
        {QStringLiteral("MR-20260725-002"), QStringLiteral("재생 가능")},
        {QStringLiteral("MR-20260725-003"), QStringLiteral("동기화 대기")}
    };
    for (int row = 0; row < queueRows.size(); ++row) {
        for (int col = 0; col < queueRows.at(row).size(); ++col) {
            queueTable->setItem(row, col, new QTableWidgetItem(queueRows.at(row).at(col)));
        }
    }
    summaryLayout->addWidget(queueTable, 1);
    leftLayout->addWidget(summaryPanel, 1);
    layout->addWidget(leftPanel, 2);

    auto *rightPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(16, 16, 16, 16);
    rightLayout->setSpacing(12);
    rightLayout->addWidget(makeLabel(QStringLiteral("최근 로그"), QStringLiteral("panelTitle")));
    m_userLogTable = new QTableWidget(0, 3, rightPanel);
    m_userLogTable->setHorizontalHeaderLabels({QStringLiteral("시각"), QStringLiteral("유형"), QStringLiteral("내용")});
    m_userLogTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_userLogTable->verticalHeader()->setVisible(false);
    m_userLogTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userLogTable->setSelectionMode(QAbstractItemView::NoSelection);
    rightLayout->addWidget(m_userLogTable, 1);
    layout->addWidget(rightPanel, 1);
    return page;
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

void DashboardCentralWidget::switchUserSection(int index)
{
    if (m_userStack == nullptr || index < 0 || index >= m_userNavButtons.size()) {
        return;
    }

    m_userStack->setCurrentIndex(index);

    for (int buttonIndex = 0; buttonIndex < m_userNavButtons.size(); ++buttonIndex) {
        QPushButton *button = m_userNavButtons.at(buttonIndex);
        button->setObjectName(buttonIndex == index
                                  ? QStringLiteral("userNavButtonActive")
                                  : QStringLiteral("userNavButton"));
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
    if (m_userAssignedRobotValue == nullptr
        || m_userBatteryValue == nullptr
        || m_userStateValue == nullptr
        || m_userMissionCountValue == nullptr
        || m_userConnectionValue == nullptr
        || m_userRobotIdLabel == nullptr
        || m_userRobotSubtitleLabel == nullptr
        || m_userRobotStatusLabel == nullptr
        || m_userHeartbeatLabel == nullptr
        || m_userHealthTagsLabel == nullptr
        || m_userMissionLogLabel == nullptr
        || m_userDescriptionLabel == nullptr
        || m_userOnlineBadge == nullptr) {
        return;
    }

    const QString robotName = snapshot.robotName.isEmpty() ? QStringLiteral("MR-02") : snapshot.robotName;
    const QString rosText = snapshot.rosText.isEmpty() ? QStringLiteral("WAIT") : snapshot.rosText;
    const QString delayText = snapshot.connectionDelayText.isEmpty() ? QStringLiteral("--") : snapshot.connectionDelayText;
    const QString dbText = snapshot.dbText.isEmpty() ? QStringLiteral("DB WAIT") : snapshot.dbText;
    const QString modeText = snapshot.mode.isEmpty() ? QStringLiteral("READY") : snapshot.mode;
    const QString stopReason = snapshot.stopReason.isEmpty() ? QStringLiteral("이상 없음") : snapshot.stopReason;

    m_userAssignedRobotValue->setText(robotName);
    m_userBatteryValue->setText(snapshot.batteryPercentText.isEmpty() ? QStringLiteral("94%") : snapshot.batteryPercentText);
    m_userStateValue->setText(snapshot.missionState == QStringLiteral("IDLE") ? QStringLiteral("READY") : snapshot.missionState);
    m_userMissionCountValue->setText(QStringLiteral("05"));
    m_userConnectionValue->setText(QStringLiteral("%1 / %2").arg(rosText, delayText));
    m_userRobotIdLabel->setText(robotName);
    m_userRobotSubtitleLabel->setText(QStringLiteral("%1 계정 세션에 연결된 운용 로봇").arg(m_session.username));
    m_userHeartbeatLabel->setText(QStringLiteral("%1 · %2").arg(rosText, delayText));
    m_userRobotStatusLabel->setText(QStringLiteral("ROS2 %1 · NAV2 %2 · DB %3").arg(rosText, modeText, dbText));
    m_userMissionLogLabel->setText(formatRecentMissionLines(snapshot.recentEvents));
    m_userHealthTagsLabel->setText(QStringLiteral("%1 · %2").arg(stopReason, dbText));
    m_userDescriptionLabel->setText(QStringLiteral("%1의 담당 로봇 %2 상태와 임무를 안전하게 관리합니다.").arg(m_session.username, robotName));
    m_userOnlineBadge->setText(QStringLiteral("%1 ONLINE").arg(robotName));

    if (m_userRobotPageTitleLabel != nullptr) {
        m_userRobotPageTitleLabel->setText(QStringLiteral("%1 Robot").arg(robotName));
    }
    if (m_userRobotDetailLabel != nullptr) {
        m_userRobotDetailLabel->setText(QStringLiteral("미션 %1 · 목적지 %2 · 버튼 %3")
                                            .arg(snapshot.missionId, snapshot.destination, snapshot.buttonId));
    }

    if (m_userRobotDeviceTable != nullptr) {
        m_userRobotDeviceTable->setRowCount(snapshot.deviceStates.size());
        for (int row = 0; row < snapshot.deviceStates.size(); ++row) {
            const auto &device = snapshot.deviceStates.at(row);
            m_userRobotDeviceTable->setItem(row, 0, new QTableWidgetItem(device.first));
            m_userRobotDeviceTable->setItem(row, 1, new QTableWidgetItem(device.second));
        }
    }

    if (m_userSensorTable != nullptr) {
        const QList<QList<QString>> sensorRows = {
            {QStringLiteral("Payload"), snapshot.payloadText, QStringLiteral("LIVE")},
            {QStringLiteral("Stop Reason"), stopReason, snapshot.mode == QStringLiteral("E-STOP") ? QStringLiteral("WARN") : QStringLiteral("CLEAR")},
            {QStringLiteral("ROS"), rosText, snapshot.rosState == ConnectionState::Disconnected ? QStringLiteral("DOWN") : QStringLiteral("OK")},
            {QStringLiteral("Network"), snapshot.networkText, snapshot.networkState == ConnectionState::Warning ? QStringLiteral("WARN") : QStringLiteral("OK")},
            {QStringLiteral("DB"), dbText, snapshot.dbState == ConnectionState::Warning ? QStringLiteral("WARN") : QStringLiteral("OK")},
            {QStringLiteral("Auto Resume"), snapshot.autoResumeCountdownText, QStringLiteral("WAIT")}
        };
        m_userSensorTable->setRowCount(sensorRows.size());
        for (int row = 0; row < sensorRows.size(); ++row) {
            for (int col = 0; col < sensorRows.at(row).size(); ++col) {
                m_userSensorTable->setItem(row, col, new QTableWidgetItem(sensorRows.at(row).at(col)));
            }
        }
    }

    if (m_userCameraStatusLabel != nullptr) {
        m_userCameraStatusLabel->setText(snapshot.mode == QStringLiteral("E-STOP") ? QStringLiteral("PAUSED") : QStringLiteral("LIVE"));
    }
    if (m_userCameraTopicLabel != nullptr) {
        m_userCameraTopicLabel->setText(QStringLiteral("%1 · %2").arg(snapshot.destination, snapshot.positionText));
    }
    if (m_userCameraEventsLabel != nullptr && !snapshot.recentEvents.isEmpty()) {
        m_userCameraEventsLabel->setText(snapshot.recentEvents.first().message);
    }

    if (m_userMapMissionLabel != nullptr) {
        m_userMapMissionLabel->setText(QStringLiteral("%1 · %2").arg(snapshot.missionId, snapshot.missionState));
    }
    if (m_userMapPositionLabel != nullptr) {
        m_userMapPositionLabel->setText(snapshot.positionText);
    }
    if (m_userMapDistanceLabel != nullptr) {
        m_userMapDistanceLabel->setText(QStringLiteral("%1 / %2").arg(snapshot.remainingDistanceText, snapshot.elapsedTimeText));
    }

    if (m_userSshStatusLabel != nullptr) {
        m_userSshStatusLabel->setText(rosText);
    }
    if (m_userSshNetworkLabel != nullptr) {
        m_userSshNetworkLabel->setText(QStringLiteral("%1 · %2").arg(snapshot.networkText, delayText));
    }
    if (m_userSshDbLabel != nullptr) {
        m_userSshDbLabel->setText(dbText);
    }

    if (m_userLogTable != nullptr) {
        m_userLogTable->setRowCount(snapshot.recentEvents.size());
        for (int row = 0; row < snapshot.recentEvents.size(); ++row) {
            const auto &event = snapshot.recentEvents.at(row);
            m_userLogTable->setItem(row, 0, new QTableWidgetItem(event.occurredAt.toString(QStringLiteral("hh:mm:ss"))));
            m_userLogTable->setItem(row, 1, new QTableWidgetItem(event.eventType));
            m_userLogTable->setItem(row, 2, new QTableWidgetItem(event.message));
        }
    }
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
