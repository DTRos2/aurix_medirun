#include "waypointmanagementpage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

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

// Minimal reader for ROS2 map_server output (binary PGM / "P5"). Qt has no
// bundled PNM plugin, so grayscale occupancy-grid maps need manual parsing.
QImage loadPgmImage(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QImage();
    }

    auto skipWhitespaceAndComments = [&file]() {
        while (!file.atEnd()) {
            char c = 0;
            if (!file.getChar(&c)) {
                break;
            }
            if (c == '#') {
                file.readLine();
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                continue;
            }
            file.ungetChar(c);
            break;
        }
    };

    auto readToken = [&file, &skipWhitespaceAndComments]() {
        skipWhitespaceAndComments();
        QByteArray token;
        char c = 0;
        while (file.getChar(&c)) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                break;
            }
            token.append(c);
        }
        return token;
    };

    if (readToken() != QByteArrayLiteral("P5")) {
        return QImage();
    }

    bool widthOk = false;
    bool heightOk = false;
    bool maxValOk = false;
    const int width = readToken().toInt(&widthOk);
    const int height = readToken().toInt(&heightOk);
    const int maxVal = readToken().toInt(&maxValOk);
    if (!widthOk || !heightOk || !maxValOk || width <= 0 || height <= 0 || maxVal <= 0 || maxVal > 255) {
        return QImage();
    }

    const QByteArray raster = file.read(static_cast<qint64>(width) * height);
    if (raster.size() != width * height) {
        return QImage();
    }

    QImage image(width, height, QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        std::memcpy(image.scanLine(y), raster.constData() + y * width, width);
    }
    return image;
}
} // namespace

WaypointManagementPage::WaypointManagementPage(QWidget *parent)
    : QWidget(parent)
    , m_creatingNew(false)
    , m_waypointTabButton(nullptr)
    , m_trajectoryTabButton(nullptr)
    , m_tabStack(nullptr)
    , m_mapStatusLabel(nullptr)
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
    , m_waypointStatusLabel(nullptr)
    , m_routeMap(nullptr)
    , m_routeAddCombo(nullptr)
    , m_routeOrderList(nullptr)
    , m_routeOrderCountLabel(nullptr)
    , m_routeNameEdit(nullptr)
    , m_savedRoutesBar(nullptr)
    , m_trajectoryStatusLabel(nullptr)
{
    m_waypoints = {
        {QStringLiteral("BTN_01"), QStringLiteral("중앙 대합실"), 4.20, 1.10, 0.0, WaypointMarkerKind::CallZone, QPointF(0.378, 0.190)},
        {QStringLiteral("BTN_02"), QStringLiteral("동측 승강장"), 9.80, 3.40, 90.0, WaypointMarkerKind::CallZone, QPointF(0.768, 0.405)},
        {QStringLiteral("BTN_03"), QStringLiteral("개찰구"), 1.60, -0.80, 180.0, WaypointMarkerKind::CallZone, QPointF(0.140, 0.619)},
        {QStringLiteral("W-04"), QStringLiteral("출입구 경유점"), 6.00, -1.20, 0.0, WaypointMarkerKind::Waypoint, QPointF(0.530, 0.714)},
        {QStringLiteral("W-05"), QStringLiteral("통로 경유점"), 3.42, -1.08, 90.0, WaypointMarkerKind::Waypoint, QPointF(0.378, 0.738)},
        {QStringLiteral("HOME"), QStringLiteral("충전 스테이션"), 0.00, 0.00, 270.0, WaypointMarkerKind::Home, QPointF(0.872, 0.857)},
    };
    m_savedRoutes = {
        {QStringLiteral("first_test"), 4},
        {QStringLiteral("patrol_a"), 6},
        {QStringLiteral("delivery_b2"), 3},
    };

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

    refreshWaypointList();
    refreshMapMarkers();
    refreshRouteCombo();
    renumberRouteOrderItems();
    refreshSavedRoutesBar();
    selectTab(0);
    selectWaypoint(QStringLiteral("W-05"));
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

    auto *text = makeLabel(QStringLiteral("모든 로봇이 대기 상태일 때만 편집 권장 · 저장 후 재기동 시 반영됩니다"));
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
    m_waypointMap->setCaption(QStringLiteral("지도 클릭 → 위치 지정 · 마커 드래그 → 좌표 조정"));
    m_waypointMap->setLegend(QStringLiteral("⊙ 호출 구역   ○ 경유점   ⌂ 홈"));
    connect(m_waypointMap, &WaypointMapWidget::markerClicked, this, &WaypointManagementPage::selectWaypoint);
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

    layout->addSpacing(4);
    auto *yamlButton = new QPushButton(QStringLiteral("YAML 저장 (재기동 필요)"), panel);
    yamlButton->setObjectName(QStringLiteral("amberFilledButton"));
    yamlButton->setStyleSheet(QStringLiteral("font-size: 11px;"));
    connect(yamlButton, &QPushButton::clicked, this, [this]() {
        m_waypointStatusLabel->setText(QStringLiteral("웨이포인트 설정을 YAML로 저장했습니다. 반영하려면 재기동하세요."));
    });
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
    for (const WaypointRecord &wp : m_waypoints) {
        auto *item = new QListWidgetItem(m_waypointList);
        item->setData(Qt::UserRole, wp.id);
        item->setSizeHint(QSize(0, 30));

        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(6);

        auto *nameLabel = makeLabel(QStringLiteral("%1  %2").arg(wp.id, wp.name));
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
    for (const WaypointRecord &wp : m_waypoints) {
        markers.append({wp.id, wp.id, wp.normalizedPos, wp.kind});
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
    for (const WaypointRecord &wp : m_waypoints) {
        if (!inRoute.contains(wp.id)) {
            m_routeAddCombo->addItem(QStringLiteral("%1  %2").arg(wp.id, wp.name), wp.id);
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

        QString name = id;
        for (const WaypointRecord &wp : m_waypoints) {
            if (wp.id == id) {
                name = wp.name;
                break;
            }
        }
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
    while ((child = layout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    layout->addWidget(makeLabel(QStringLiteral("저장된 경로"), QStringLiteral("metricCaption")));

    for (const SavedRoute &route : m_savedRoutes) {
        auto *chip = new QPushButton(QStringLiteral("%1 · %2점").arg(route.name).arg(route.pointCount), m_savedRoutesBar);
        chip->setObjectName(route.name == m_selectedSavedRoute ? QStringLiteral("routeChipActive") : QStringLiteral("routeChip"));
        connect(chip, &QPushButton::clicked, this, [this, name = route.name]() { handleSavedRouteChipClicked(name); });
        layout->addWidget(chip);
    }

    layout->addStretch();

    auto *deleteButton = new QPushButton(QStringLiteral("선택 삭제"), m_savedRoutesBar);
    deleteButton->setObjectName(QStringLiteral("inlineGhostButton"));
    connect(deleteButton, &QPushButton::clicked, this, &WaypointManagementPage::handleDeleteSelectedSavedRoute);
    layout->addWidget(deleteButton);
}

void WaypointManagementPage::selectWaypoint(const QString &id)
{
    for (const WaypointRecord &wp : m_waypoints) {
        if (wp.id != id) {
            continue;
        }
        m_creatingNew = false;
        m_selectedWaypointId = id;
        loadWaypointIntoForm(wp);
        m_waypointMap->setSelectedId(id);

        for (int i = 0; i < m_waypointList->count(); ++i) {
            QListWidgetItem *item = m_waypointList->item(i);
            if (item->data(Qt::UserRole).toString() == id) {
                m_waypointList->setCurrentItem(item);
                break;
            }
        }
        return;
    }
}

void WaypointManagementPage::loadWaypointIntoForm(const WaypointRecord &record)
{
    m_editPanelTitle->setText(QStringLiteral("%1 편집").arg(record.id));
    m_nameEdit->setText(record.name);
    m_xEdit->setText(QString::number(record.x, 'f', 2));
    m_xEdit->setCursorPosition(0);
    m_yEdit->setText(QString::number(record.y, 'f', 2));
    m_yEdit->setCursorPosition(0);
    m_thetaEdit->setText(QString::number(record.theta, 'f', 0));
    m_thetaEdit->setCursorPosition(0);
    m_kindCombo->setCurrentIndex(m_kindCombo->findData(static_cast<int>(record.kind)));
    m_saveWaypointButton->setText(QStringLiteral("저장"));
    m_deleteWaypointButton->setEnabled(record.id != QStringLiteral("HOME"));
}

void WaypointManagementPage::handleNewWaypointRequested()
{
    m_creatingNew = true;
    m_selectedWaypointId.clear();
    m_waypointList->clearSelection();
    m_waypointMap->setSelectedId(QString());

    m_editPanelTitle->setText(QStringLiteral("새 웨이포인트 추가"));
    m_nameEdit->clear();
    m_xEdit->setText(QStringLiteral("0.00"));
    m_yEdit->setText(QStringLiteral("0.00"));
    m_thetaEdit->setText(QStringLiteral("0"));
    m_kindCombo->setCurrentIndex(m_kindCombo->findData(static_cast<int>(WaypointMarkerKind::Waypoint)));
    m_saveWaypointButton->setText(QStringLiteral("웨이포인트 추가"));
    m_deleteWaypointButton->setEnabled(false);
    m_nameEdit->setFocus();
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
    const double theta = m_thetaEdit->text().trimmed().toDouble();
    if (!xOk || !yOk) {
        m_waypointStatusLabel->setText(QStringLiteral("X, Y 좌표는 숫자로 입력해야 합니다."));
        return;
    }

    const auto kind = static_cast<WaypointMarkerKind>(m_kindCombo->currentData().toInt());

    if (m_creatingNew) {
        if (kind == WaypointMarkerKind::Home) {
            const bool homeExists = std::any_of(m_waypoints.cbegin(), m_waypoints.cend(),
                                                 [](const WaypointRecord &wp) { return wp.kind == WaypointMarkerKind::Home; });
            if (homeExists) {
                m_waypointStatusLabel->setText(QStringLiteral("HOME은 하나만 존재할 수 있습니다."));
                return;
            }
        }

        WaypointRecord record;
        record.id = nextIdForKind(kind);
        record.name = name;
        record.x = x;
        record.y = y;
        record.theta = theta;
        record.kind = kind;
        record.normalizedPos = normalizedFromMeters(x, y);
        m_waypoints.append(record);
        m_selectedWaypointId = record.id;
        m_creatingNew = false;
        m_waypointStatusLabel->setText(QStringLiteral("%1 웨이포인트를 추가했습니다.").arg(record.id));
    } else {
        for (WaypointRecord &wp : m_waypoints) {
            if (wp.id == m_selectedWaypointId) {
                wp.name = name;
                wp.x = x;
                wp.y = y;
                wp.theta = theta;
                wp.kind = kind;
                wp.normalizedPos = normalizedFromMeters(x, y);
                break;
            }
        }
        m_waypointStatusLabel->setText(QStringLiteral("%1 웨이포인트를 수정했습니다.").arg(m_selectedWaypointId));
    }

    refreshWaypointList();
    refreshMapMarkers();
    refreshRouteCombo();
    renumberRouteOrderItems();
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
    m_waypoints.erase(std::remove_if(m_waypoints.begin(), m_waypoints.end(),
                                      [&](const WaypointRecord &wp) { return wp.id == deletedId; }),
                       m_waypoints.end());

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

    if (!m_waypoints.isEmpty()) {
        selectWaypoint(m_waypoints.first().id);
    } else {
        handleNewWaypointRequested();
    }
}

QString WaypointManagementPage::nextIdForKind(WaypointMarkerKind kind) const
{
    if (kind == WaypointMarkerKind::Home) {
        return QStringLiteral("HOME");
    }

    int maxNum = 0;
    for (const WaypointRecord &wp : m_waypoints) {
        if (wp.kind != kind) {
            continue;
        }
        if (kind == WaypointMarkerKind::CallZone && wp.id.startsWith(QStringLiteral("BTN_"))) {
            maxNum = std::max(maxNum, wp.id.mid(4).toInt());
        } else if (kind == WaypointMarkerKind::Waypoint && wp.id.startsWith(QStringLiteral("W-"))) {
            maxNum = std::max(maxNum, wp.id.mid(2).toInt());
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
    const bool nameTaken = std::any_of(m_savedRoutes.cbegin(), m_savedRoutes.cend(), [&](const SavedRoute &route) {
        return route.name.compare(name, Qt::CaseInsensitive) == 0;
    });
    if (nameTaken) {
        m_trajectoryStatusLabel->setText(QStringLiteral("이미 존재하는 경로 이름입니다."));
        return;
    }

    m_savedRoutes.append({name, m_routeOrderList->count()});
    m_trajectoryStatusLabel->setText(QStringLiteral("%1 경로를 Trajectory로 저장했습니다.").arg(name));

    m_routeNameEdit->clear();
    m_routeOrderList->clear();
    refreshRouteCombo();
    renumberRouteOrderItems();
    refreshSavedRoutesBar();
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
    m_savedRoutes.erase(std::remove_if(m_savedRoutes.begin(), m_savedRoutes.end(),
                                        [&](const SavedRoute &route) { return route.name == deletedName; }),
                         m_savedRoutes.end());
    m_selectedSavedRoute.clear();
    m_trajectoryStatusLabel->setText(QStringLiteral("%1 경로를 삭제했습니다.").arg(deletedName));
    refreshSavedRoutesBar();
}

void WaypointManagementPage::handleLoadMap()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("지도 불러오기"), QString(),
        QStringLiteral("지도 파일 (*.pgm *.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) {
        return;
    }

    QImage image;
    if (path.endsWith(QStringLiteral(".pgm"), Qt::CaseInsensitive)) {
        image = loadPgmImage(path);
    } else {
        image.load(path);
    }

    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("지도 불러오기 실패"),
                              QStringLiteral("지도 파일을 읽을 수 없습니다. PGM(P5)·PNG·JPG·BMP 형식만 지원합니다."));
        return;
    }

    m_waypointMap->setBackgroundImage(image);
    m_routeMap->setBackgroundImage(image);
    m_mapStatusLabel->setText(QStringLiteral("불러온 지도: %1").arg(QFileInfo(path).fileName()));
}

void WaypointManagementPage::handleClearMap()
{
    m_waypointMap->clearBackgroundImage();
    m_routeMap->clearBackgroundImage();
    m_mapStatusLabel->setText(QStringLiteral("불러온 지도 없음"));
}
