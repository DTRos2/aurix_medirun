#include "waypointmanagementpage.h"

#include "../services/waypointrepository.h"
#include "../services/waypointfile.h"
#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QThread>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>

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

QString kindLabel(WaypointMarkerKind kind)
{
    switch (kind) {
    case WaypointMarkerKind::CallZone:
        return QStringLiteral("호출");
    case WaypointMarkerKind::Waypoint:
        return QStringLiteral("경유");
    case WaypointMarkerKind::Home:
        return QStringLiteral("홈");
    }
    return QString();
}
} // namespace

WaypointManagementPage::WaypointManagementPage(QWidget *parent)
    : QWidget(parent)
    , m_creatingNew(false)
    , m_settingInitialPose(false)
    , m_waypointTabButton(nullptr)
    , m_trajectoryTabButton(nullptr)
    , m_tabStack(nullptr)
    , m_mapStatusLabel(nullptr)
    , m_navStatusLabel(nullptr)
    , m_setInitialPoseButton(nullptr)
    , m_speedButton(nullptr)
    , m_waypointMap(nullptr)
    , m_waypointList(nullptr)
    , m_waypointListCountLabel(nullptr)
    , m_editPanelTitle(nullptr)
    , m_nameEdit(nullptr)
    , m_xEdit(nullptr)
    , m_yEdit(nullptr)
    , m_thetaEdit(nullptr)
    , m_kindCombo(nullptr)
    , m_saveWaypointButton(nullptr)
    , m_deleteWaypointButton(nullptr)
    , m_navigateButton(nullptr)
    , m_cancelNavButton(nullptr)
    , m_waypointStatusLabel(nullptr)
    , m_routeMap(nullptr)
    , m_routeAddCombo(nullptr)
    , m_routeOrderList(nullptr)
    , m_routeOrderCountLabel(nullptr)
    , m_routeNameEdit(nullptr)
    , m_savedRoutesBar(nullptr)
    , m_navigateRouteButton(nullptr)
    , m_trajectoryStatusLabel(nullptr)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto *canvas = panelFrame(QStringLiteral("pageCanvas"));
    outerLayout->addWidget(canvas);

    auto *layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    layout->addWidget(buildHeader());
    layout->addWidget(buildWarningBanner());

    m_tabStack = new QStackedWidget(canvas);
    m_tabStack->addWidget(buildWaypointTab());
    m_tabStack->addWidget(buildTrajectoryTab());
    layout->addWidget(m_tabStack, 1);

    autoLoadDefaultMap();
    refreshWaypointList();
    refreshMapMarkers();
    refreshRouteCombo();
    renumberRouteOrderItems();
    refreshSavedRoutesBar();
    selectTab(0);
    updateNavButtonsEnabled();
    handleNewWaypointRequested();
}

void WaypointManagementPage::setNavigationService(NavigationService *service)
{
    m_navigationService = service;
    if (m_navigationService == nullptr) {
        handleNav2ConnectionChanged(false);
        updateNavButtonsEnabled();
        return;
    }

    connect(m_navigationService, &NavigationService::nav2ConnectionChanged, this, &WaypointManagementPage::handleNav2ConnectionChanged);
    connect(m_navigationService, &NavigationService::statusChanged, this, &WaypointManagementPage::handleNavStatusChanged);
    connect(m_navigationService, &NavigationService::feedbackDistanceRemaining, this, &WaypointManagementPage::handleNavFeedback);
    connect(m_navigationService, &NavigationService::waypointProgress, this, &WaypointManagementPage::handleNavWaypointProgress);
    connect(m_navigationService, &NavigationService::amclPoseUpdated, this, &WaypointManagementPage::handleAmclPoseUpdated);
    connect(m_navigationService, &NavigationService::scanPointsUpdated, this, &WaypointManagementPage::handleScanPointsUpdated);
    connect(m_navigationService, &NavigationService::logEvent, this, [this](const QString &level, const QString &message) {
        if (level == QStringLiteral("ERROR") || level == QStringLiteral("WARN")) {
            m_navStatusLabel->setText(QStringLiteral("[%1] %2").arg(level, message));
        }
    });

    updateNavButtonsEnabled();
}

void WaypointManagementPage::setWaypointRepository(WaypointRepository *repository)
{
    if (m_waypointRepository == repository) {
        return;
    }
    m_waypointRepository = repository;
    if (m_waypointRepository == nullptr) {
        return;
    }

    connect(m_waypointRepository, &WaypointRepository::repositoryChanged, this, &WaypointManagementPage::syncFromRepository);
    syncFromRepository();
}

void WaypointManagementPage::syncFromRepository()
{
    if (m_waypointRepository == nullptr) {
        return;
    }

    m_frameId = m_waypointRepository->frameId();
    m_waypoints = m_waypointRepository->waypoints();
    m_trajectories = m_waypointRepository->trajectories();
    m_waypointFilePath = m_waypointRepository->filePath();

    refreshWaypointList();
    refreshMapMarkers();
    refreshRouteCombo();
    renumberRouteOrderItems();
    refreshSavedRoutesBar();

    if (!m_selectedWaypointId.isEmpty() && m_waypoints.contains(m_selectedWaypointId)) {
        selectWaypoint(m_selectedWaypointId);
    } else if (!m_waypoints.isEmpty()) {
        selectWaypoint(m_waypoints.firstKey());
    } else {
        handleNewWaypointRequested();
    }
}

void WaypointManagementPage::pushToRepository()
{
    if (m_waypointRepository == nullptr) {
        return;
    }
    m_waypointRepository->replaceAll(m_frameId, m_waypoints, m_trajectories, m_waypointFilePath);
}

QWidget *WaypointManagementPage::buildHeader()
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(2);
    titleCol->addWidget(makeLabel(QStringLiteral("웨이포인트 · 경로 관리"), QStringLiteral("panelTitle")));
    auto *subtitleLabel = makeLabel(
        QStringLiteral("호출 구역·경유점·홈 위치를 등록하고, 등록된 웨이포인트를 엮어 주행 경로를 만듭니다."),
        QStringLiteral("pageSubtitle"));
    subtitleLabel->setWordWrap(false);
    titleCol->addWidget(subtitleLabel);
    layout->addLayout(titleCol);
    layout->addStretch();

    m_nav2ConnectionBadge = makeBadgeLabel(QStringLiteral("Nav2 미연결"), QStringLiteral("red"), row);
    layout->addWidget(m_nav2ConnectionBadge);

    m_navStatusLabel = makeLabel(QString(), QStringLiteral("metricCaption"));
    m_navStatusLabel->setWordWrap(false);
    layout->addWidget(m_navStatusLabel);

    m_setInitialPoseButton = new QPushButton(QStringLiteral("초기 위치 설정"), row);
    m_setInitialPoseButton->setObjectName(QStringLiteral("ghostChipButton"));
    m_setInitialPoseButton->setCheckable(true);
    m_setInitialPoseButton->setToolTip(
        QStringLiteral("RViz의 2D Pose Estimate와 동일 · 켠 뒤 지도를 클릭+드래그하면 그 위치·방향으로 AMCL 초기 위치를 설정합니다"));
    connect(m_setInitialPoseButton, &QPushButton::toggled, this, &WaypointManagementPage::handleSetInitialPoseToggled);
    layout->addWidget(m_setInitialPoseButton);

    m_speedButton = new QPushButton(QStringLiteral("주행 속도 100%"), row);
    m_speedButton->setObjectName(QStringLiteral("ghostChipButton"));
    m_speedButton->setToolTip(QStringLiteral("웨이포인트 주행 속도 제한 (Nav2 컨트롤러 최대 속도 대비 %)"));
    connect(m_speedButton, &QPushButton::clicked, this, &WaypointManagementPage::showSpeedPopup);
    layout->addWidget(m_speedButton);

    auto *navDivider = new QFrame(row);
    navDivider->setObjectName(QStringLiteral("sectionDivider"));
    navDivider->setFixedWidth(1);
    navDivider->setFixedHeight(22);
    layout->addWidget(navDivider);

    auto *loadWpButton = new QPushButton(QStringLiteral("웨이포인트 불러오기"), row);
    loadWpButton->setObjectName(QStringLiteral("ghostChipButton"));
    connect(loadWpButton, &QPushButton::clicked, this, &WaypointManagementPage::handleLoadWaypointFile);
    layout->addWidget(loadWpButton);

    auto *clearMapButton = new QPushButton(QStringLiteral("지도 초기화"), row);
    clearMapButton->setObjectName(QStringLiteral("ghostChipButton"));
    connect(clearMapButton, &QPushButton::clicked, this, &WaypointManagementPage::handleClearMap);
    layout->addWidget(clearMapButton);

    auto *loadMapButton = new QPushButton(QStringLiteral("지도 불러오기"), row);
    loadMapButton->setObjectName(QStringLiteral("cyanChipButton"));
    connect(loadMapButton, &QPushButton::clicked, this, &WaypointManagementPage::handleLoadMap);
    layout->addWidget(loadMapButton);

    m_mapStatusLabel = makeLabel(QStringLiteral("불러온 지도 없음"), QStringLiteral("metricCaption"));
    layout->addWidget(m_mapStatusLabel);

    auto *headerDivider = new QFrame(row);
    headerDivider->setObjectName(QStringLiteral("sectionDivider"));
    headerDivider->setFixedWidth(1);
    headerDivider->setFixedHeight(22);
    layout->addWidget(headerDivider);

    auto *tabSwitcher = new QFrame(row);
    tabSwitcher->setObjectName(QStringLiteral("tabSwitcher"));
    auto *tabLayout = new QHBoxLayout(tabSwitcher);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(6);

    m_waypointTabButton = new QPushButton(QStringLiteral("웨이포인트"), tabSwitcher);
    m_trajectoryTabButton = new QPushButton(QStringLiteral("경로 만들기"), tabSwitcher);
    for (QPushButton *button : {m_waypointTabButton, m_trajectoryTabButton}) {
        button->setObjectName(QStringLiteral("tabButton"));
        tabLayout->addWidget(button);
    }
    connect(m_waypointTabButton, &QPushButton::clicked, this, [this]() { selectTab(0); });
    connect(m_trajectoryTabButton, &QPushButton::clicked, this, [this]() { selectTab(1); });
    layout->addWidget(tabSwitcher);

    return row;
}

QWidget *WaypointManagementPage::buildWarningBanner()
{
    auto *panel = panelFrame(QStringLiteral("warningBanner"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto *icon = makeLabel(QStringLiteral("⚠"));
    icon->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(DashboardPalette::Amber.name()));
    layout->addWidget(icon);

    auto *text = makeLabel(QStringLiteral("모든 로봇이 대기 상태일 때만 편집 권장 · 저장한 파일은 dispatcher 재기동 후 반영됩니다"));
    text->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(DashboardPalette::Amber.name()));
    layout->addWidget(text, 1);

    return panel;
}

QWidget *WaypointManagementPage::buildWaypointTab()
{
    auto *tab = new QWidget;
    auto *bodyRow = new QHBoxLayout(tab);
    bodyRow->setContentsMargins(0, 0, 0, 0);
    bodyRow->setSpacing(10);

    m_waypointMap = new WaypointMapWidget(tab);
    m_waypointMap->setMinimumHeight(250);
    m_waypointMap->setCaption(QStringLiteral("지도 클릭 → 웨이포인트 생성 · 드래그 → 방향 지정 · 마커 클릭 → 편집"));
    m_waypointMap->setLegend(QStringLiteral("⊙ 호출 구역   ○ 경유점   ⌂ 홈"));
    connect(m_waypointMap, &WaypointMapWidget::markerClicked, this, &WaypointManagementPage::selectWaypoint);
    connect(m_waypointMap, &WaypointMapWidget::waypointPickRequested, this, &WaypointManagementPage::handleMapPickRequested);
    bodyRow->addWidget(m_waypointMap, 7);

    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);
    rightCol->addWidget(buildWaypointListPanel(), 1);
    rightCol->addWidget(buildWaypointEditPanel());

    auto *rightColWidget = new QWidget(tab);
    rightColWidget->setFixedWidth(230);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    return tab;
}

QWidget *WaypointManagementPage::buildWaypointListPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    m_waypointListCountLabel = makeLabel(QStringLiteral("웨이포인트 · 0개"), QStringLiteral("metricCaption"));
    headerRow->addWidget(m_waypointListCountLabel, 1);
    auto *newButton = new QPushButton(QStringLiteral("+ 새 웨이포인트"), panel);
    newButton->setObjectName(QStringLiteral("cyanChipButton"));
    connect(newButton, &QPushButton::clicked, this, &WaypointManagementPage::handleNewWaypointRequested);
    headerRow->addWidget(newButton);
    layout->addLayout(headerRow);

    m_waypointList = new QListWidget(panel);
    connect(m_waypointList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        selectWaypoint(item->data(Qt::UserRole).toString());
    });
    layout->addWidget(m_waypointList, 1);

    return panel;
}

QWidget *WaypointManagementPage::buildWaypointEditPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    m_editPanelTitle = makeLabel(QStringLiteral("편집"), QStringLiteral("panelTitle"));
    layout->addWidget(m_editPanelTitle);

    layout->addWidget(makeLabel(QStringLiteral("이름"), QStringLiteral("fieldLabel")));
    m_nameEdit = new QLineEdit(panel);
    layout->addWidget(m_nameEdit);

    auto *coordRow = new QHBoxLayout;
    coordRow->setSpacing(6);

    auto *xCol = new QVBoxLayout;
    xCol->addWidget(makeLabel(QStringLiteral("X (m)"), QStringLiteral("fieldLabel")));
    m_xEdit = new QLineEdit(panel);
    m_xEdit->setObjectName(QStringLiteral("compactField"));
    xCol->addWidget(m_xEdit);
    coordRow->addLayout(xCol);

    auto *yCol = new QVBoxLayout;
    yCol->addWidget(makeLabel(QStringLiteral("Y (m)"), QStringLiteral("fieldLabel")));
    m_yEdit = new QLineEdit(panel);
    m_yEdit->setObjectName(QStringLiteral("compactField"));
    yCol->addWidget(m_yEdit);
    coordRow->addLayout(yCol);

    auto *thetaCol = new QVBoxLayout;
    thetaCol->addWidget(makeLabel(QStringLiteral("θ (°)"), QStringLiteral("fieldLabel")));
    m_thetaEdit = new QLineEdit(panel);
    m_thetaEdit->setObjectName(QStringLiteral("compactField"));
    thetaCol->addWidget(m_thetaEdit);
    coordRow->addLayout(thetaCol);

    layout->addLayout(coordRow);

    layout->addWidget(makeLabel(QStringLiteral("유형"), QStringLiteral("fieldLabel")));
    m_kindCombo = new QComboBox(panel);
    m_kindCombo->addItem(QStringLiteral("호출 구역"), static_cast<int>(WaypointMarkerKind::CallZone));
    m_kindCombo->addItem(QStringLiteral("경유점"), static_cast<int>(WaypointMarkerKind::Waypoint));
    m_kindCombo->addItem(QStringLiteral("홈"), static_cast<int>(WaypointMarkerKind::Home));
    layout->addWidget(m_kindCombo);

    auto *actionRow = new QHBoxLayout;
    m_saveWaypointButton = new QPushButton(QStringLiteral("저장"), panel);
    m_saveWaypointButton->setObjectName(QStringLiteral("primaryCtaButton"));
    connect(m_saveWaypointButton, &QPushButton::clicked, this, &WaypointManagementPage::handleSaveWaypoint);
    actionRow->addWidget(m_saveWaypointButton, 1);

    m_deleteWaypointButton = new QPushButton(QStringLiteral("삭제"), panel);
    m_deleteWaypointButton->setObjectName(QStringLiteral("dangerFilledButton"));
    m_deleteWaypointButton->setFixedWidth(56);
    connect(m_deleteWaypointButton, &QPushButton::clicked, this, &WaypointManagementPage::handleDeleteWaypoint);
    actionRow->addWidget(m_deleteWaypointButton);
    layout->addLayout(actionRow);

    auto *navRow = new QHBoxLayout;
    m_navigateButton = new QPushButton(QStringLiteral("이 웨이포인트로 주행"), panel);
    m_navigateButton->setObjectName(QStringLiteral("cyanOutlineButton"));
    connect(m_navigateButton, &QPushButton::clicked, this, &WaypointManagementPage::handleNavigateToSelectedWaypoint);
    navRow->addWidget(m_navigateButton, 1);

    m_cancelNavButton = new QPushButton(QStringLiteral("정지"), panel);
    m_cancelNavButton->setObjectName(QStringLiteral("dangerFilledButton"));
    m_cancelNavButton->setFixedWidth(56);
    connect(m_cancelNavButton, &QPushButton::clicked, this, &WaypointManagementPage::handleCancelNavigation);
    navRow->addWidget(m_cancelNavButton);
    layout->addLayout(navRow);

    layout->addSpacing(4);
    auto *yamlButton = new QPushButton(QStringLiteral("전체 웨이포인트 저장 (YAML)"), panel);
    yamlButton->setObjectName(QStringLiteral("amberFilledButton"));
    yamlButton->setStyleSheet(QStringLiteral("font-size: 11px;"));
    connect(yamlButton, &QPushButton::clicked, this, &WaypointManagementPage::handleSaveWaypointFile);
    layout->addWidget(yamlButton);

    m_waypointStatusLabel = makeLabel(QString(), QStringLiteral("pageSubtitle"));
    layout->addWidget(m_waypointStatusLabel);

    return panel;
}

QWidget *WaypointManagementPage::buildTrajectoryTab()
{
    auto *tab = new QWidget;
    auto *outer = new QVBoxLayout(tab);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(10);

    m_routeMap = new WaypointMapWidget(tab);
    m_routeMap->setMinimumHeight(250);
    m_routeMap->setCaption(QStringLiteral("경로에 담긴 웨이포인트는 번호·연결선으로 표시됩니다"));
    m_routeMap->setLegend(QStringLiteral("① 순서   ○ 미포함 웨이포인트"));
    bodyRow->addWidget(m_routeMap, 7);

    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);
    rightCol->addWidget(buildRouteAddPanel());
    rightCol->addWidget(buildRouteOrderPanel(), 1);
    rightCol->addWidget(buildRouteNamePanel());

    auto *rightColWidget = new QWidget(tab);
    rightColWidget->setFixedWidth(230);
    rightColWidget->setLayout(rightCol);
    bodyRow->addWidget(rightColWidget);

    outer->addLayout(bodyRow, 1);
    outer->addWidget(buildSavedRoutesBar());

    return tab;
}

QWidget *WaypointManagementPage::buildRouteAddPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("경로에 추가"), QStringLiteral("metricCaption")));

    auto *row = new QHBoxLayout;
    m_routeAddCombo = new QComboBox(panel);
    row->addWidget(m_routeAddCombo, 1);

    auto *addButton = new QPushButton(QStringLiteral("추가"), panel);
    addButton->setObjectName(QStringLiteral("primaryCtaButton"));
    addButton->setFixedWidth(56);
    connect(addButton, &QPushButton::clicked, this, &WaypointManagementPage::handleAddToRoute);
    row->addWidget(addButton);
    layout->addLayout(row);

    return panel;
}

QWidget *WaypointManagementPage::buildRouteOrderPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    m_routeOrderCountLabel = makeLabel(QStringLiteral("경로 순서 · 0개"), QStringLiteral("metricCaption"));
    headerRow->addWidget(m_routeOrderCountLabel, 1);
    headerRow->addWidget(makeLabel(QStringLiteral("드래그로 순서 변경"), QStringLiteral("metricCaption")));
    layout->addLayout(headerRow);

    m_routeOrderList = new QListWidget(panel);
    m_routeOrderList->setDragDropMode(QAbstractItemView::InternalMove);
    m_routeOrderList->setDefaultDropAction(Qt::MoveAction);
    connect(m_routeOrderList->model(), &QAbstractItemModel::rowsMoved, this, &WaypointManagementPage::renumberRouteOrderItems);
    layout->addWidget(m_routeOrderList, 1);

    auto *buttonRow = new QHBoxLayout;
    auto *removeButton = new QPushButton(QStringLiteral("선택 제거"), panel);
    connect(removeButton, &QPushButton::clicked, this, &WaypointManagementPage::handleRemoveSelectedFromRoute);
    buttonRow->addWidget(removeButton);

    auto *clearButton = new QPushButton(QStringLiteral("전체 초기화"), panel);
    connect(clearButton, &QPushButton::clicked, this, &WaypointManagementPage::handleClearRoute);
    buttonRow->addWidget(clearButton);
    layout->addLayout(buttonRow);

    return panel;
}

QWidget *WaypointManagementPage::buildRouteNamePanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("경로 이름"), QStringLiteral("metricCaption")));

    m_routeNameEdit = new QLineEdit(panel);
    m_routeNameEdit->setPlaceholderText(QStringLiteral("예: patrol_a"));
    layout->addWidget(m_routeNameEdit);

    auto *saveButton = new QPushButton(QStringLiteral("Trajectory로 저장"), panel);
    saveButton->setObjectName(QStringLiteral("primaryCtaButton"));
    connect(saveButton, &QPushButton::clicked, this, &WaypointManagementPage::handleSaveTrajectory);
    layout->addWidget(saveButton);

    m_trajectoryStatusLabel = makeLabel(QString(), QStringLiteral("pageSubtitle"));
    layout->addWidget(m_trajectoryStatusLabel);

    return panel;
}

QWidget *WaypointManagementPage::buildSavedRoutesBar()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(8);
    m_savedRoutesBar = panel;

    m_navigateRouteButton = new QPushButton(QStringLiteral("선택 경로 주행"), panel);
    m_navigateRouteButton->setObjectName(QStringLiteral("cyanOutlineButton"));
    connect(m_navigateRouteButton, &QPushButton::clicked, this, &WaypointManagementPage::handleNavigateSelectedRoute);

    auto *cancelButton = new QPushButton(QStringLiteral("정지"), panel);
    cancelButton->setObjectName(QStringLiteral("dangerFilledButton"));
    connect(cancelButton, &QPushButton::clicked, this, &WaypointManagementPage::handleCancelNavigation);

    layout->addWidget(m_navigateRouteButton);
    layout->addWidget(cancelButton);

    return panel;
}

void WaypointManagementPage::selectTab(int index)
{
    m_tabStack->setCurrentIndex(index);
    m_waypointTabButton->setObjectName(index == 0 ? QStringLiteral("tabButtonActive") : QStringLiteral("tabButton"));
    m_trajectoryTabButton->setObjectName(index == 1 ? QStringLiteral("tabButtonActive") : QStringLiteral("tabButton"));
    for (QPushButton *button : {m_waypointTabButton, m_trajectoryTabButton}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void WaypointManagementPage::refreshWaypointList()
{
    m_waypointList->clear();
    const QStringList ids = m_waypoints.keys();
    for (const QString &id : ids) {
        const WaypointDef &wp = m_waypoints.value(id);
        auto *item = new QListWidgetItem(m_waypointList);
        item->setData(Qt::UserRole, id);
        item->setSizeHint(QSize(0, 30));

        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(6);

        auto *nameLabel = makeLabel(QStringLiteral("%1  %2").arg(id, wp.label));
        nameLabel->setStyleSheet(QStringLiteral("font-size: 11px;"));
        rowLayout->addWidget(nameLabel, 1);
        rowLayout->addWidget(makeBadgeLabel(kindLabel(wp.kind), QStringLiteral("muted"), row));

        m_waypointList->setItemWidget(item, row);
    }
    m_waypointListCountLabel->setText(QStringLiteral("웨이포인트 · %1개").arg(m_waypoints.size()));
}

void WaypointManagementPage::refreshMapMarkers()
{
    QVector<WaypointMapMarker> markers;
    const QStringList ids = m_waypoints.keys();
    for (const QString &id : ids) {
        const WaypointDef &wp = m_waypoints.value(id);
        markers.append({id, id, worldToNormalized(wp.x, wp.y), wp.kind});
    }
    m_waypointMap->setMarkers(markers);
    m_routeMap->setMarkers(markers);
}

void WaypointManagementPage::refreshRouteCombo()
{
    QSet<QString> inRoute;
    for (int i = 0; i < m_routeOrderList->count(); ++i) {
        inRoute.insert(m_routeOrderList->item(i)->data(Qt::UserRole).toString());
    }

    const QString previousSelection = m_routeAddCombo->currentData().toString();
    m_routeAddCombo->clear();
    const QStringList ids = m_waypoints.keys();
    for (const QString &id : ids) {
        if (!inRoute.contains(id)) {
            m_routeAddCombo->addItem(QStringLiteral("%1  %2").arg(id, m_waypoints.value(id).label), id);
        }
    }
    const int idx = m_routeAddCombo->findData(previousSelection);
    if (idx >= 0) {
        m_routeAddCombo->setCurrentIndex(idx);
    }
}

void WaypointManagementPage::renumberRouteOrderItems()
{
    static const QStringList circled = {
        QStringLiteral("①"), QStringLiteral("②"), QStringLiteral("③"), QStringLiteral("④"), QStringLiteral("⑤"),
        QStringLiteral("⑥"), QStringLiteral("⑦"), QStringLiteral("⑧"), QStringLiteral("⑨"), QStringLiteral("⑩"),
    };

    QVector<QString> order;
    for (int i = 0; i < m_routeOrderList->count(); ++i) {
        QListWidgetItem *item = m_routeOrderList->item(i);
        const QString id = item->data(Qt::UserRole).toString();
        order.append(id);

        const QString name = m_waypoints.contains(id) ? m_waypoints.value(id).label : id;
        const QString number = i < circled.size() ? circled.at(i) : QString::number(i + 1);
        item->setText(QStringLiteral("%1  %2  %3").arg(number, id, name));
    }

    m_routeOrderCountLabel->setText(QStringLiteral("경로 순서 · %1개").arg(order.size()));
    m_routeMap->setRouteOrder(order);
}

void WaypointManagementPage::refreshSavedRoutesBar()
{
    auto *layout = qobject_cast<QHBoxLayout *>(m_savedRoutesBar->layout());
    QLayoutItem *child = nullptr;
    QVector<QLayoutItem *> keep;
    // 처음 두 아이템(주행/정지 버튼)은 그대로 두고 나머지(칩 목록)만 다시 그린다.
    QWidget *navButton = layout->takeAt(0)->widget();
    QWidget *cancelButton = layout->takeAt(0)->widget();
    while ((child = layout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    layout->addWidget(navButton);
    layout->addWidget(cancelButton);

    auto *divider = new QFrame(m_savedRoutesBar);
    divider->setObjectName(QStringLiteral("sectionDivider"));
    divider->setFixedWidth(1);
    divider->setFixedHeight(22);
    layout->addWidget(divider);

    layout->addWidget(makeLabel(QStringLiteral("저장된 경로"), QStringLiteral("metricCaption")));

    const QStringList names = m_trajectories.keys();
    for (const QString &name : names) {
        auto *chip = new QPushButton(QStringLiteral("%1 · %2점").arg(name).arg(m_trajectories.value(name).size()), m_savedRoutesBar);
        chip->setObjectName(name == m_selectedSavedRoute ? QStringLiteral("routeChipActive") : QStringLiteral("routeChip"));
        connect(chip, &QPushButton::clicked, this, [this, name]() { handleSavedRouteChipClicked(name); });
        layout->addWidget(chip);
    }

    layout->addStretch();

    auto *deleteButton = new QPushButton(QStringLiteral("선택 삭제"), m_savedRoutesBar);
    deleteButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(deleteButton, &QPushButton::clicked, this, &WaypointManagementPage::handleDeleteSelectedSavedRoute);
    layout->addWidget(deleteButton);

    updateNavButtonsEnabled();
}

void WaypointManagementPage::selectWaypoint(const QString &id)
{
    if (!m_waypoints.contains(id)) {
        return;
    }

    m_creatingNew = false;
    m_selectedWaypointId = id;
    loadWaypointIntoForm(m_waypoints.value(id));
    m_waypointMap->setSelectedId(id);

    for (int i = 0; i < m_waypointList->count(); ++i) {
        QListWidgetItem *item = m_waypointList->item(i);
        if (item->data(Qt::UserRole).toString() == id) {
            m_waypointList->setCurrentItem(item);
            break;
        }
    }
    updateNavButtonsEnabled();
}

void WaypointManagementPage::loadWaypointIntoForm(const WaypointDef &record)
{
    m_editPanelTitle->setText(QStringLiteral("%1 편집").arg(record.name));
    m_nameEdit->setText(record.label);
    m_xEdit->setText(QString::number(record.x, 'f', 2));
    m_xEdit->setCursorPosition(0);
    m_yEdit->setText(QString::number(record.y, 'f', 2));
    m_yEdit->setCursorPosition(0);
    m_thetaEdit->setText(QString::number(qRadiansToDegrees(record.yaw), 'f', 0));
    m_thetaEdit->setCursorPosition(0);
    m_kindCombo->setCurrentIndex(m_kindCombo->findData(static_cast<int>(record.kind)));
    m_saveWaypointButton->setText(QStringLiteral("저장"));
    m_deleteWaypointButton->setEnabled(record.name != QStringLiteral("HOME"));
}

void WaypointManagementPage::handleNewWaypointRequested()
{
    prefillNewWaypointForm(0.0, 0.0, 0.0);
}

void WaypointManagementPage::prefillNewWaypointForm(double x, double y, double yawDeg)
{
    m_creatingNew = true;
    m_selectedWaypointId.clear();
    m_waypointList->clearSelection();
    m_waypointMap->setSelectedId(QString());

    m_editPanelTitle->setText(QStringLiteral("새 웨이포인트 추가"));
    m_nameEdit->clear();
    m_xEdit->setText(QString::number(x, 'f', 2));
    m_yEdit->setText(QString::number(y, 'f', 2));
    m_thetaEdit->setText(QString::number(yawDeg, 'f', 0));
    m_kindCombo->setCurrentIndex(m_kindCombo->findData(static_cast<int>(WaypointMarkerKind::Waypoint)));
    m_saveWaypointButton->setText(QStringLiteral("웨이포인트 추가"));
    m_deleteWaypointButton->setEnabled(false);
    m_nameEdit->setFocus();
    updateNavButtonsEnabled();
}

void WaypointManagementPage::handleSaveWaypoint()
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_waypointStatusLabel->setText(QStringLiteral("이름을 입력하세요."));
        return;
    }

    bool xOk = false;
    bool yOk = false;
    const double x = m_xEdit->text().trimmed().toDouble(&xOk);
    const double y = m_yEdit->text().trimmed().toDouble(&yOk);
    const double thetaDeg = m_thetaEdit->text().trimmed().toDouble();
    if (!xOk || !yOk) {
        m_waypointStatusLabel->setText(QStringLiteral("X, Y 좌표는 숫자로 입력해야 합니다."));
        return;
    }

    const auto kind = static_cast<WaypointMarkerKind>(m_kindCombo->currentData().toInt());

    if (m_creatingNew) {
        if (kind == WaypointMarkerKind::Home) {
            const bool homeExists = std::any_of(m_waypoints.cbegin(), m_waypoints.cend(),
                                                 [](const WaypointDef &wp) { return wp.kind == WaypointMarkerKind::Home; });
            if (homeExists) {
                m_waypointStatusLabel->setText(QStringLiteral("HOME은 하나만 존재할 수 있습니다."));
                return;
            }
        }

        WaypointDef record;
        record.name = nextIdForKind(kind);
        record.label = name;
        record.x = x;
        record.y = y;
        record.yaw = qDegreesToRadians(thetaDeg);
        record.kind = kind;
        m_waypoints.insert(record.name, record);
        m_selectedWaypointId = record.name;
        m_creatingNew = false;
        m_waypointStatusLabel->setText(QStringLiteral("%1 웨이포인트를 추가했습니다.").arg(record.name));
    } else {
        WaypointDef &wp = m_waypoints[m_selectedWaypointId];
        wp.label = name;
        wp.x = x;
        wp.y = y;
        wp.yaw = qDegreesToRadians(thetaDeg);
        wp.kind = kind;
        m_waypointStatusLabel->setText(QStringLiteral("%1 웨이포인트를 수정했습니다.").arg(m_selectedWaypointId));
    }

    refreshWaypointList();
    refreshMapMarkers();
    refreshRouteCombo();
    renumberRouteOrderItems();
    pushToRepository();
    selectWaypoint(m_selectedWaypointId);
}

void WaypointManagementPage::handleDeleteWaypoint()
{
    if (m_selectedWaypointId.isEmpty()) {
        return;
    }
    if (m_selectedWaypointId == QStringLiteral("HOME")) {
        m_waypointStatusLabel->setText(QStringLiteral("HOME은 삭제할 수 없습니다."));
        return;
    }

    const auto confirm = QMessageBox::question(
        this, QStringLiteral("웨이포인트 삭제"),
        QStringLiteral("%1 웨이포인트를 삭제하시겠습니까?").arg(m_selectedWaypointId));
    if (confirm != QMessageBox::Yes) {
        return;
    }

    const QString deletedId = m_selectedWaypointId;
    m_waypoints.remove(deletedId);

    for (int i = m_routeOrderList->count() - 1; i >= 0; --i) {
        if (m_routeOrderList->item(i)->data(Qt::UserRole).toString() == deletedId) {
            delete m_routeOrderList->takeItem(i);
        }
    }

    m_waypointStatusLabel->setText(QStringLiteral("%1 웨이포인트를 삭제했습니다.").arg(deletedId));
    refreshWaypointList();
    refreshMapMarkers();
    refreshRouteCombo();
    renumberRouteOrderItems();
    pushToRepository();

    if (!m_waypoints.isEmpty()) {
        selectWaypoint(m_waypoints.firstKey());
    } else {
        handleNewWaypointRequested();
    }
}

void WaypointManagementPage::handleMapPickRequested(const QPointF &normalizedPos, double yawRadians)
{
    if (!m_currentMapMeta.valid) {
        m_waypointStatusLabel->setText(QStringLiteral("먼저 지도를 불러오세요."));
        return;
    }
    const QPointF world = normalizedToWorld(normalizedPos);

    if (m_settingInitialPose) {
        // Unchecking triggers handleSetInitialPoseToggled(false), which
        // resets the flag and restores the waypoint-picking caption.
        m_setInitialPoseButton->setChecked(false);
        if (m_navigationService == nullptr) {
            m_waypointStatusLabel->setText(QStringLiteral("Nav2 연동이 준비되지 않았습니다."));
            return;
        }
        m_navigationService->publishInitialPose(world.x(), world.y(), yawRadians, m_frameId);
        m_waypointStatusLabel->setText(QStringLiteral("초기 위치를 설정했습니다: x=%1, y=%2")
                                            .arg(world.x(), 0, 'f', 2)
                                            .arg(world.y(), 0, 'f', 2));
        return;
    }

    prefillNewWaypointForm(world.x(), world.y(), qRadiansToDegrees(yawRadians));
}

void WaypointManagementPage::handleSetInitialPoseToggled(bool checked)
{
    m_settingInitialPose = checked;
    if (checked) {
        selectTab(0);
        m_waypointMap->setCaption(
            QStringLiteral("지도 클릭 + 드래그 → 로봇의 실제 위치·방향으로 초기 위치 설정 (RViz 2D Pose Estimate와 동일)"));
    } else {
        m_waypointMap->setCaption(QStringLiteral("지도 클릭 → 웨이포인트 생성 · 드래그 → 방향 지정 · 마커 클릭 → 편집"));
    }
}

void WaypointManagementPage::showSpeedPopup()
{
    // Qt::Popup auto-closes on outside click/focus loss — matches the
    // "click the chip → adjust in a small tooltip-like panel" behavior
    // requested, without needing a modal dialog.
    auto *popup = new QWidget(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName(QStringLiteral("speedPopup"));
    popup->setFixedWidth(190);

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);

    layout->addWidget(makeLabel(QStringLiteral("웨이포인트 주행 속도"), QStringLiteral("metricCaption")));

    auto *valueLabel = makeLabel(QStringLiteral("%1%").arg(m_speedPercent), QStringLiteral("panelTitle"));
    valueLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(valueLabel);

    auto *slider = new QSlider(Qt::Horizontal, popup);
    slider->setRange(10, 100);
    slider->setSingleStep(5);
    slider->setPageStep(10);
    slider->setValue(m_speedPercent);
    layout->addWidget(slider);

    connect(slider, &QSlider::valueChanged, this, [this, valueLabel](int value) {
        valueLabel->setText(QStringLiteral("%1%").arg(value));
        applySpeedLimit(value);
    });

    popup->move(m_speedButton->mapToGlobal(QPoint(0, m_speedButton->height() + 4)));
    popup->show();
}

void WaypointManagementPage::applySpeedLimit(int percent)
{
    m_speedPercent = percent;
    m_speedButton->setText(QStringLiteral("주행 속도 %1%").arg(percent));
    if (m_navigationService != nullptr) {
        m_navigationService->setSpeedLimit(percent);
    }
}

QString WaypointManagementPage::nextIdForKind(WaypointMarkerKind kind) const
{
    if (kind == WaypointMarkerKind::Home) {
        return QStringLiteral("HOME");
    }

    int maxNum = 0;
    const QStringList ids = m_waypoints.keys();
    for (const QString &id : ids) {
        const WaypointMarkerKind existingKind = m_waypoints.value(id).kind;
        if (existingKind != kind) {
            continue;
        }
        if (kind == WaypointMarkerKind::CallZone && id.startsWith(QStringLiteral("BTN_"))) {
            maxNum = std::max(maxNum, id.mid(4).toInt());
        } else if (kind == WaypointMarkerKind::Waypoint && id.startsWith(QStringLiteral("W-"))) {
            maxNum = std::max(maxNum, id.mid(2).toInt());
        }
    }

    if (kind == WaypointMarkerKind::CallZone) {
        return QStringLiteral("BTN_%1").arg(maxNum + 1, 2, 10, QChar('0'));
    }
    return QStringLiteral("W-%1").arg(maxNum + 1, 2, 10, QChar('0'));
}

QPointF WaypointManagementPage::normalizedFromMeters(double x, double y) const
{
    const double nx = std::clamp(0.5 + x / 20.0, 0.06, 0.94);
    const double ny = std::clamp(0.5 - y / 12.0, 0.06, 0.94);
    return QPointF(nx, ny);
}

QPointF WaypointManagementPage::worldToNormalized(double x, double y) const
{
    if (!m_currentMapMeta.valid) {
        return normalizedFromMeters(x, y);
    }
    const QPointF pixel = worldToPixel(QPointF(x, y), m_currentMapMeta);
    return QPointF(pixel.x() / m_currentMapMeta.width, pixel.y() / m_currentMapMeta.height);
}

QPointF WaypointManagementPage::normalizedToWorld(const QPointF &normalized) const
{
    if (!m_currentMapMeta.valid) {
        // 지도 없이 호출되지 않도록 handleMapPickRequested에서 가드하지만,
        // 방어적으로 역변환 근사치를 반환한다.
        const double x = (normalized.x() - 0.5) * 20.0;
        const double y = (0.5 - normalized.y()) * 12.0;
        return QPointF(x, y);
    }
    const QPointF pixel(normalized.x() * m_currentMapMeta.width, normalized.y() * m_currentMapMeta.height);
    return pixelToWorld(pixel, m_currentMapMeta);
}

void WaypointManagementPage::handleAddToRoute()
{
    const QString id = m_routeAddCombo->currentData().toString();
    if (id.isEmpty()) {
        return;
    }

    auto *item = new QListWidgetItem;
    item->setData(Qt::UserRole, id);
    m_routeOrderList->addItem(item);

    refreshRouteCombo();
    renumberRouteOrderItems();
    m_trajectoryStatusLabel->setText(QStringLiteral("%1 을(를) 경로에 추가했습니다.").arg(id));
}

void WaypointManagementPage::handleRemoveSelectedFromRoute()
{
    QListWidgetItem *item = m_routeOrderList->currentItem();
    if (!item) {
        m_trajectoryStatusLabel->setText(QStringLiteral("제거할 항목을 목록에서 선택하세요."));
        return;
    }
    delete m_routeOrderList->takeItem(m_routeOrderList->row(item));
    refreshRouteCombo();
    renumberRouteOrderItems();
}

void WaypointManagementPage::handleClearRoute()
{
    m_routeOrderList->clear();
    refreshRouteCombo();
    renumberRouteOrderItems();
    m_trajectoryStatusLabel->setText(QStringLiteral("경로를 초기화했습니다."));
}

void WaypointManagementPage::handleSaveTrajectory()
{
    const QString name = m_routeNameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_trajectoryStatusLabel->setText(QStringLiteral("경로 이름을 입력하세요."));
        return;
    }
    if (m_routeOrderList->count() < 2) {
        m_trajectoryStatusLabel->setText(QStringLiteral("경로에는 웨이포인트가 2개 이상 필요합니다."));
        return;
    }
    if (m_trajectories.contains(name)) {
        m_trajectoryStatusLabel->setText(QStringLiteral("이미 존재하는 경로 이름입니다."));
        return;
    }

    QStringList sequence;
    for (int i = 0; i < m_routeOrderList->count(); ++i) {
        sequence.append(m_routeOrderList->item(i)->data(Qt::UserRole).toString());
    }
    m_trajectories.insert(name, sequence);
    m_trajectoryStatusLabel->setText(QStringLiteral("%1 경로를 Trajectory로 저장했습니다.").arg(name));

    m_routeNameEdit->clear();
    m_routeOrderList->clear();
    refreshRouteCombo();
    renumberRouteOrderItems();
    refreshSavedRoutesBar();
    pushToRepository();
}

void WaypointManagementPage::handleSavedRouteChipClicked(const QString &name)
{
    m_selectedSavedRoute = (m_selectedSavedRoute == name) ? QString() : name;
    refreshSavedRoutesBar();
}

void WaypointManagementPage::handleDeleteSelectedSavedRoute()
{
    if (m_selectedSavedRoute.isEmpty()) {
        m_trajectoryStatusLabel->setText(QStringLiteral("삭제할 저장된 경로를 선택하세요."));
        return;
    }

    const QString deletedName = m_selectedSavedRoute;
    m_trajectories.remove(deletedName);
    m_selectedSavedRoute.clear();
    m_trajectoryStatusLabel->setText(QStringLiteral("%1 경로를 삭제했습니다.").arg(deletedName));
    refreshSavedRoutesBar();
    pushToRepository();
}

void WaypointManagementPage::handleLoadMap()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("지도 불러오기 (nav2 map_server YAML)"), QString(),
        QStringLiteral("맵 YAML (*.yaml *.yml)"));
    if (path.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!loadMapFromFile(path, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("지도 불러오기 실패"), errorMessage);
        return;
    }
    if (!errorMessage.isEmpty()) {
        // 회전 warning 등 — 실패는 아니었으므로 로드는 계속 진행한다.
        m_waypointStatusLabel->setText(errorMessage);
    }
}

bool WaypointManagementPage::loadMapFromFile(const QString &path, QString &errorMessage)
{
    MapMeta meta;
    if (!loadMapYaml(path, meta, errorMessage)) {
        return false;
    }

    m_currentMapMeta = meta;
    m_waypointMap->setMapMeta(meta);
    m_waypointMap->setBackgroundImage(meta.image);
    m_routeMap->setMapMeta(meta);
    m_routeMap->setBackgroundImage(meta.image);

    m_mapStatusLabel->setText(QStringLiteral("불러온 지도: %1 (%2×%3, %4 m/px)")
                                   .arg(QFileInfo(path).fileName())
                                   .arg(meta.width)
                                   .arg(meta.height)
                                   .arg(meta.resolution));
    refreshMapMarkers();
    return true;
}

QString WaypointManagementPage::workspaceRootPath() const
{
    // 실행 파일 경로에서 고정 단계 수만큼 올라가는 방식은 --symlink-install
    // 여부에 따라 실제 깊이가 달라져 신뢰할 수 없다: OS가 install/의
    // 심볼릭 링크를 실행 전에 미리 풀어버리면 QCoreApplication::
    // applicationDirPath()가 install/medirun_gui/lib/medirun_gui가 아니라
    // 그 링크가 가리키는 build/medirun_gui를 반환해 깊이가 2단계 차이 남.
    // 대신 maps/ 디렉터리가 나올 때까지 상위로 탐색한다(계정별 절대 경로
    // 하드코딩 금지, CONVENTIONS.md 3.2절 관례는 유지).
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("maps")))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

void WaypointManagementPage::autoLoadDefaultMap()
{
    const QString workspaceRoot = workspaceRootPath();
    if (workspaceRoot.isEmpty()) {
        return;
    }

    const QString defaultMapPath = QDir(workspaceRoot).filePath(QStringLiteral("maps/last_class_map_modi.yaml"));
    if (!QFileInfo::exists(defaultMapPath)) {
        return;
    }

    QString errorMessage;
    if (loadMapFromFile(defaultMapPath, errorMessage) && !errorMessage.isEmpty()) {
        m_waypointStatusLabel->setText(errorMessage);
    }
}

void WaypointManagementPage::handleClearMap()
{
    m_waypointMap->clearBackgroundImage();
    m_routeMap->clearBackgroundImage();
    m_currentMapMeta = MapMeta();
    m_mapStatusLabel->setText(QStringLiteral("불러온 지도 없음"));
    refreshMapMarkers();
}

void WaypointManagementPage::handleLoadWaypointFile()
{
    QString initialPath = m_waypointFilePath;
    if (initialPath.isEmpty()) {
        const QString workspaceRoot = workspaceRootPath();
        if (!workspaceRoot.isEmpty()) {
            initialPath = QDir(workspaceRoot).filePath(QStringLiteral("maps/waypoints"));
        }
    }

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("웨이포인트 파일 불러오기"), initialPath,
        QStringLiteral("YAML 파일 (*.yaml *.yml)"));
    if (path.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (m_waypointRepository != nullptr) {
        if (!m_waypointRepository->loadFromFile(path, errorMessage)) {
            QMessageBox::warning(this, QStringLiteral("웨이포인트 불러오기 실패"), errorMessage);
            return;
        }
        return;
    }

    QString frameId;
    WaypointMap waypoints;
    TrajectoryMap trajectories;
    if (!loadWaypointYaml(path, frameId, waypoints, trajectories, errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("웨이포인트 불러오기 실패"), errorMessage);
        return;
    }

    m_frameId = frameId;
    m_waypoints = waypoints;
    m_trajectories = trajectories;
    m_waypointFilePath = path;
    m_selectedSavedRoute.clear();
    m_routeOrderList->clear();

    refreshWaypointList();
    refreshMapMarkers();
    refreshRouteCombo();
    renumberRouteOrderItems();
    refreshSavedRoutesBar();

    m_waypointStatusLabel->setText(
        QStringLiteral("'%1'에서 웨이포인트 %2개, 경로 %3개를 불러왔습니다.")
            .arg(QFileInfo(path).fileName())
            .arg(m_waypoints.size())
            .arg(m_trajectories.size()));

    if (!m_waypoints.isEmpty()) {
        selectWaypoint(m_waypoints.firstKey());
    } else {
        handleNewWaypointRequested();
    }
}

void WaypointManagementPage::handleSaveWaypointFile()
{
    if (m_waypoints.isEmpty()) {
        m_waypointStatusLabel->setText(QStringLiteral("저장할 웨이포인트가 없습니다."));
        return;
    }

    QString initialPath = m_waypointFilePath;
    if (initialPath.isEmpty()) {
        const QString workspaceRoot = workspaceRootPath();
        if (!workspaceRoot.isEmpty()) {
            initialPath = QDir(workspaceRoot).filePath(QStringLiteral("maps/waypoints"));
        }
    }

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("웨이포인트 파일 저장"), initialPath,
        QStringLiteral("YAML 파일 (*.yaml *.yml)"));
    if (path.isEmpty()) {
        return;
    }
    // 파일 이름에 확장자를 안 붙이면(Qt가 자동으로 붙여주지 않는 플랫폼이
    // 있음) "불러오기" 대화상자의 *.yaml/*.yml 필터에 안 걸려 목록에서
    // 안 보이게 된다 — 저장 시점에 강제로 붙여준다.
    if (!path.endsWith(QStringLiteral(".yaml"), Qt::CaseInsensitive)
        && !path.endsWith(QStringLiteral(".yml"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".yaml");
    }

    QString errorMessage;
    if (!saveWaypointYaml(path, m_frameId, m_waypoints, m_trajectories, errorMessage)) {
        if (m_waypointRepository == nullptr || !m_waypointRepository->saveToFile(path, errorMessage)) {
            QMessageBox::warning(this, QStringLiteral("웨이포인트 저장 실패"), errorMessage);
            return;
        }
    } else if (m_waypointRepository != nullptr) {
        m_waypointRepository->saveToFile(path, errorMessage);
    }

    m_waypointFilePath = path;
    m_waypointStatusLabel->setText(QStringLiteral("웨이포인트 파일을 저장했습니다: %1").arg(path));
    pushToRepository();
}

void WaypointManagementPage::updateNavButtonsEnabled()
{
    const bool hasService = (m_navigationService != nullptr);
    // 실제 주행에는 navigate_to_pose/follow_waypoints 액션 서버가 떠 있어야
    // 한다(m_nav2Connected) — SLAM 매핑 중 PAUSE된 동안이나 콜드부트 직후에는
    // hasService(rclcpp 노드 연결)는 true여도 nav2 자체는 아직 준비 안 됐을 수
    // 있다. 초기 위치 설정은 AMCL(localization)만 있으면 되므로 nav2Ready와
    // 무관하게 hasService만으로 활성화한다.
    const bool nav2Ready = hasService && m_nav2Connected;
    if (m_navigateButton != nullptr) {
        m_navigateButton->setEnabled(nav2Ready && !m_creatingNew && !m_selectedWaypointId.isEmpty());
        m_navigateButton->setText(nav2Ready ? QStringLiteral("이 웨이포인트로 주행") : QStringLiteral("Nav2 미연결"));
    }
    if (m_cancelNavButton != nullptr) {
        m_cancelNavButton->setEnabled(hasService);
    }
    if (m_navigateRouteButton != nullptr) {
        m_navigateRouteButton->setEnabled(nav2Ready && !m_selectedSavedRoute.isEmpty());
        m_navigateRouteButton->setText(nav2Ready ? QStringLiteral("선택 경로 주행") : QStringLiteral("Nav2 미연결"));
    }
    if (m_setInitialPoseButton != nullptr) {
        m_setInitialPoseButton->setEnabled(hasService);
        if (!hasService) {
            m_setInitialPoseButton->setChecked(false);
        }
    }
}

void WaypointManagementPage::handleNavigateToSelectedWaypoint()
{
    if (m_navigationService == nullptr) {
        m_waypointStatusLabel->setText(QStringLiteral("Nav2 연동이 준비되지 않았습니다."));
        return;
    }
    // 버튼이 비활성화돼 있어야 정상이지만(updateNavButtonsEnabled), 클릭과
    // 연결 끊김이 겹치는 race를 대비해 여기서도 한 번 더 막는다.
    if (!m_nav2Connected) {
        m_waypointStatusLabel->setText(QStringLiteral("Nav2가 아직 연결되지 않았습니다."));
        return;
    }
    if (m_creatingNew || !m_waypoints.contains(m_selectedWaypointId)) {
        m_waypointStatusLabel->setText(QStringLiteral("먼저 저장된 웨이포인트를 선택하세요."));
        return;
    }

    const WaypointDef &wp = m_waypoints.value(m_selectedWaypointId);
    m_navigationService->navigateToPose(wp.x, wp.y, wp.yaw, m_frameId);
}

void WaypointManagementPage::handleNavigateSelectedRoute()
{
    if (m_navigationService == nullptr) {
        m_trajectoryStatusLabel->setText(QStringLiteral("Nav2 연동이 준비되지 않았습니다."));
        return;
    }
    if (!m_nav2Connected) {
        m_trajectoryStatusLabel->setText(QStringLiteral("Nav2가 아직 연결되지 않았습니다."));
        return;
    }
    if (m_selectedSavedRoute.isEmpty() || !m_trajectories.contains(m_selectedSavedRoute)) {
        m_trajectoryStatusLabel->setText(QStringLiteral("주행할 경로를 먼저 선택하세요."));
        return;
    }

    QVector<NavPose> poses;
    for (const QString &wpId : m_trajectories.value(m_selectedSavedRoute)) {
        if (!m_waypoints.contains(wpId)) {
            continue;
        }
        const WaypointDef &wp = m_waypoints.value(wpId);
        poses.append({wp.x, wp.y, wp.yaw});
    }
    if (poses.isEmpty()) {
        m_trajectoryStatusLabel->setText(QStringLiteral("경로에 유효한 웨이포인트가 없습니다."));
        return;
    }
    m_navigationService->followWaypoints(poses, m_frameId);
}

void WaypointManagementPage::handleCancelNavigation()
{
    if (m_navigationService == nullptr) {
        return;
    }
    m_navigationService->cancelGoal();
}

void WaypointManagementPage::handleNavStatusChanged(const QString &text)
{
    m_navStatusLabel->setText(text);
}

void WaypointManagementPage::handleNav2ConnectionChanged(bool connected)
{
    m_nav2Connected = connected;
    if (m_nav2ConnectionBadge != nullptr) {
        m_nav2ConnectionBadge->setText(connected ? QStringLiteral("Nav2 연결") : QStringLiteral("Nav2 미연결"));
        m_nav2ConnectionBadge->setProperty("badgeKind", connected ? QStringLiteral("green") : QStringLiteral("red"));
        m_nav2ConnectionBadge->style()->unpolish(m_nav2ConnectionBadge);
        m_nav2ConnectionBadge->style()->polish(m_nav2ConnectionBadge);
    }
    updateNavButtonsEnabled();
}

void WaypointManagementPage::handleNavFeedback(double metersRemaining)
{
    m_navStatusLabel->setText(QStringLiteral("목적지까지 남은 거리: %1 m").arg(metersRemaining, 0, 'f', 2));
}

void WaypointManagementPage::handleNavWaypointProgress(int currentIndex, int total)
{
    m_navStatusLabel->setText(QStringLiteral("경로 주행 중 (%1/%2)").arg(currentIndex).arg(total));
}

void WaypointManagementPage::handleAmclPoseUpdated(double x, double y, double yawRad)
{
    if (!m_currentMapMeta.valid) {
        return;
    }
    const QPointF normalized = worldToNormalized(x, y);
    m_waypointMap->setRobotPose(normalized, yawRad, true);
    m_routeMap->setRobotPose(normalized, yawRad, true);
}

void WaypointManagementPage::handleScanPointsUpdated(const QVector<QPointF> &mapPoints)
{
    if (!m_currentMapMeta.valid) {
        return;
    }

    QVector<QPointF> normalized;
    normalized.reserve(mapPoints.size());
    for (const QPointF &point : mapPoints) {
        normalized.append(worldToNormalized(point.x(), point.y()));
    }

    m_waypointMap->setScanPoints(normalized);
    m_routeMap->setScanPoints(normalized);
}
