#include "operationsoverviewpage.h"

#include "../services/operationsoverviewservice.h"
#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/proportionalbar.h"

#include <QAbstractItemView>
#include <QDate>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kRefreshIntervalMs = 5000;

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

void applyBadgeKind(QLabel *label, const QString &kind, const QString &text)
{
    label->setText(text);
    label->setProperty("badgeKind", kind);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

QColor colorForKind(const QString &kind)
{
    if (kind == QStringLiteral("cyan")) {
        return DashboardPalette::Cyan;
    }
    if (kind == QStringLiteral("green")) {
        return DashboardPalette::Green;
    }
    if (kind == QStringLiteral("amber")) {
        return DashboardPalette::Amber;
    }
    if (kind == QStringLiteral("red")) {
        return DashboardPalette::Red;
    }
    return DashboardPalette::TextMuted;
}

// addWidget()된 자식들을 지워 이후 addWidget()으로 다시 채울 수 있게 한다.
// 행을 addLayout()이 아닌 QWidget으로 감싸 두었기 때문에 위젯 삭제만으로
// 소유한 하위 레이아웃/위젯까지 함께 정리된다.
void clearLayout(QVBoxLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

// 카메라는 DB에 남는 값이 아니라 실시간 신호 하나(연결됨/안됨)뿐이라 퍼센트
// 막대와 같은 모양을 쓰되 0/100으로만 표시한다. 신호를 아직 한 번도 못
// 받았으면(hasSignal=false) "미확인" 의미로 회색 빈 막대를 보여준다.
SystemHealthRow liveHealthRow(const QString &label, bool hasSignal, bool connected)
{
    if (!hasSignal) {
        return {label, 0, QStringLiteral("muted")};
    }
    return {label, connected ? 100 : 0, connected ? QStringLiteral("green") : QStringLiteral("red")};
}

// Nav2는 "액션 서버 ACTIVE"만으로는 실제 주행 가능 여부를 말해주지 못해서
// (AMCL이 초기 위치를 못 받았으면 goal은 받아도 주행이 실패/오동작할 수
// 있음) 3단계로 나눠 보여준다. 상태 이름을 라벨에 직접 적어서, 퍼센트 숫자를
// "진행률"이 아니라 단계 표시 정도로만 곁들인다.
SystemHealthRow nav2HealthRow(bool hasSignal, bool connected, bool hasPose)
{
    if (!hasSignal) {
        return {QStringLiteral("Nav2 — 확인 중"), 0, QStringLiteral("muted")};
    }
    if (!connected) {
        return {QStringLiteral("Nav2 — 부팅 중"), 0, QStringLiteral("amber")};
    }
    if (!hasPose) {
        return {QStringLiteral("Nav2 — 위치 설정 필요"), 50, QStringLiteral("amber")};
    }
    return {QStringLiteral("Nav2 — 주행 가능"), 100, QStringLiteral("green")};
}

// nav2navigationservice.cpp가 실제로 내보내는 문구("...로 이동 중...",
// "경로 주행 중 (i/N)", "대기 중", "대기 중 (마지막 결과: ...)", "취소
// 요청됨...")를 그대로 색으로 매핑한다.
QString kindForNavStatusText(const QString &text)
{
    if (text.contains(QStringLiteral("이동 중")) || text.contains(QStringLiteral("주행 중"))) {
        return QStringLiteral("cyan");
    }
    if (text.contains(QStringLiteral("취소"))) {
        return QStringLiteral("amber");
    }
    return QStringLiteral("muted");
}

// "목적지 (12.34, 5.67)로 이동 중...", "경로 주행 중 (2/5)"처럼 좌표/진행률이
// 섞인 원문 대신 로봇 상태 칸에는 짧게 통일해서 보여준다.
QString shortNavStatusLabel(const QString &text)
{
    if (text.contains(QStringLiteral("이동 중")) || text.contains(QStringLiteral("주행 중"))) {
        return QStringLiteral("주행");
    }
    if (text.contains(QStringLiteral("취소"))) {
        return QStringLiteral("취소 중");
    }
    return QStringLiteral("대기");
}

QWidget *buildHealthRow(const SystemHealthRow &row)
{
    auto *rowWidget = new QWidget;
    auto *rowLayout = new QVBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    auto *labelRow = new QHBoxLayout;
    labelRow->addWidget(makeLabel(row.label, QStringLiteral("healthLabel")));
    labelRow->addStretch();
    labelRow->addWidget(makeLabel(QStringLiteral("%1%").arg(row.percent), QStringLiteral("healthValue")));
    rowLayout->addLayout(labelRow);
    rowLayout->addWidget(makeProportionalBar(row.percent, colorForKind(row.kind), rowWidget));
    return rowWidget;
}

QWidget *buildEventRow(const OperationsEventRow &event)
{
    auto *rowWidget = new QWidget;
    auto *row = new QHBoxLayout(rowWidget);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    auto *bar = new QFrame(rowWidget);
    bar->setFixedSize(3, 20);
    bar->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 1px;").arg(colorForKind(event.kind).name()));
    row->addWidget(bar);
    row->addWidget(makeLabel(event.occurredAt.toString(QStringLiteral("HH:mm")), QStringLiteral("metricCaption")));
    row->addWidget(makeLabel(event.message, QStringLiteral("pageSubtitle")), 1);
    row->addWidget(makeBadgeLabel(event.severityLabel, event.kind, rowWidget));
    return rowWidget;
}

} // namespace

OperationsOverviewPage::OperationsOverviewPage(OperationsOverviewService &service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
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

    m_metricCards.resize(4);
    metricRow->addWidget(buildMetricCard(QStringLiteral("연결 로봇"), QStringLiteral("cyan"), m_metricCards[0]), 0, 0);
    metricRow->addWidget(buildMetricCard(QStringLiteral("진행 미션"), QStringLiteral("cyan"), m_metricCards[1]), 0, 1);
    metricRow->addWidget(buildMetricCard(QStringLiteral("오늘 호출"), QStringLiteral("muted"), m_metricCards[2]), 0, 2);
    metricRow->addWidget(buildMetricCard(QStringLiteral("안전 이벤트"), QStringLiteral("amber"), m_metricCards[3]), 0, 3);
    layout->addLayout(metricRow);

    auto *midRow = new QHBoxLayout;
    midRow->setSpacing(12);
    midRow->addWidget(buildRobotStatusPanel(), 7);
    midRow->addWidget(buildHealthPanel(), 4);
    layout->addLayout(midRow);

    layout->addWidget(buildEventsPanel());

    refresh();

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &OperationsOverviewPage::refresh);
    m_refreshTimer->start(kRefreshIntervalMs);
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

    m_statusBadge = makeBadgeLabel(QStringLiteral("확인 중"), QStringLiteral("muted"), header);
    layout->addWidget(m_statusBadge);
    return header;
}

QWidget *OperationsOverviewPage::buildMetricCard(const QString &title, const QString &kind, MetricCardWidgets &widgetsOut)
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

    widgetsOut.valueLabel = makeLabel(QStringLiteral("-"), QStringLiteral("metricValue"));
    widgetsOut.captionLabel = makeLabel(QString(), QStringLiteral("metricCaption"));
    layout->addWidget(widgetsOut.valueLabel);
    layout->addWidget(widgetsOut.captionLabel);
    return card;
}

QWidget *OperationsOverviewPage::buildRobotStatusPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("실시간 로봇 상태"), QStringLiteral("panelTitle")));

    m_robotTable = new QTableWidget(0, 4, panel);
    m_robotTable->setHorizontalHeaderLabels({QStringLiteral("로봇"), QStringLiteral("상태"), QStringLiteral("배터리"), QStringLiteral("경로")});
    m_robotTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_robotTable->verticalHeader()->setVisible(false);
    m_robotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_robotTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_robotTable->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(m_robotTable);
    return panel;
}

QWidget *OperationsOverviewPage::buildHealthPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(QStringLiteral("시스템 헬스"), QStringLiteral("panelTitle")));

    auto *rowsContainer = new QWidget(panel);
    m_healthLayout = new QVBoxLayout(rowsContainer);
    m_healthLayout->setContentsMargins(0, 0, 0, 0);
    m_healthLayout->setSpacing(12);
    layout->addWidget(rowsContainer);

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

    auto *rowsContainer = new QWidget(panel);
    m_eventsLayout = new QVBoxLayout(rowsContainer);
    m_eventsLayout->setContentsMargins(0, 0, 0, 0);
    m_eventsLayout->setSpacing(10);
    layout->addWidget(rowsContainer);

    return panel;
}

void OperationsOverviewPage::refresh()
{
    refreshSummary();
    refreshRobotStatus();
    refreshHealth();
    refreshEvents();
}

void OperationsOverviewPage::refreshSummary()
{
    OperationsSummaryMetrics metrics;
    QString errorMessage;
    if (!m_service.fetchSummary(metrics, errorMessage)) {
        qWarning() << "운영 현황 요약 조회 실패:" << errorMessage;
        return;
    }

    m_metricCards[0].valueLabel->setText(QString::number(metrics.connectedRobots));
    m_metricCards[0].captionLabel->setText(QStringLiteral("등록 %1대 중 온라인").arg(metrics.registeredRobots));

    m_metricCards[1].valueLabel->setText(QString::number(metrics.activeMissions));
    m_metricCards[1].captionLabel->setText(
        metrics.activeEmergencyMissions > 0
            ? QStringLiteral("긴급 %1건 포함").arg(metrics.activeEmergencyMissions)
            : QStringLiteral("긴급 호출 없음"));

    m_metricCards[2].valueLabel->setText(QString::number(metrics.todayCalls));
    m_metricCards[2].captionLabel->setText(
        metrics.todayEmergencyCalls > 0
            ? QStringLiteral("긴급 %1건 포함").arg(metrics.todayEmergencyCalls)
            : QStringLiteral("긴급 호출 없음"));

    m_metricCards[3].valueLabel->setText(QString::number(metrics.safetyEventsLast24h));
    m_metricCards[3].captionLabel->setText(QStringLiteral("최근 24시간"));

    const bool normal = metrics.safetyEventsLast24h == 0;
    applyBadgeKind(
        m_statusBadge,
        normal ? QStringLiteral("green") : QStringLiteral("amber"),
        normal ? QStringLiteral("전체 시스템 정상") : QStringLiteral("주의 필요"));
}

void OperationsOverviewPage::refreshRobotStatus()
{
    QVector<RobotStatusRow> rows;
    QString errorMessage;
    if (!m_service.fetchRobotStatus(rows, errorMessage)) {
        qWarning() << "로봇 상태 조회 실패:" << errorMessage;
        return;
    }

    m_lastRobotRows = rows;
    renderRobotTable();
}

void OperationsOverviewPage::renderRobotTable()
{
    QVector<RobotStatusRow> rows = m_lastRobotRows;

    // 등록된 로봇이 정확히 1대일 때만 실시간 배터리/주행 상태를 덮어쓴다.
    // 여러 대면 이 신호가 어느 로봇 것인지 특정할 수 없어 DB 값(없으면 "-"/
    // "대기") 그대로 둔다.
    if (rows.size() == 1 && m_hasSnapshot && m_latestSnapshot.batteryMessageReceived) {
        rows[0].batteryText = m_latestSnapshot.batteryPercentText;
    }
    // mission/robot_session 어디에도 기록되지 않는 웨이포인트 직접 주행은
    // DB로는 절대 알 수 없어서, NavigationService::statusChanged를 그대로
    // "상태" 칸에 덮어쓴다 — 이게 없으면 실제로 주행 중이어도 항상 "대기"로
    // 보인다(사용자가 실사용 중 발견).
    if (rows.size() == 1 && m_hasNavStatus) {
        rows[0].statusLabel = QStringLiteral("● %1").arg(shortNavStatusLabel(m_liveNavStatusText));
        rows[0].statusKind = kindForNavStatusText(m_liveNavStatusText);
    }

    m_robotTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const RobotStatusRow &data = rows.at(row);
        m_robotTable->setItem(row, 0, new QTableWidgetItem(data.robotName));
        auto *statusItem = new QTableWidgetItem(data.statusLabel);
        statusItem->setForeground(colorForKind(data.statusKind));
        m_robotTable->setItem(row, 1, statusItem);
        m_robotTable->setItem(row, 2, new QTableWidgetItem(data.batteryText));
        m_robotTable->setItem(row, 3, new QTableWidgetItem(data.routeText));
    }
}

void OperationsOverviewPage::applyTelemetrySnapshot(const TelemetrySnapshot &snapshot)
{
    m_latestSnapshot = snapshot;
    m_hasSnapshot = true;
    renderRobotTable();
}

void OperationsOverviewPage::applyNavStatus(const QString &text)
{
    m_liveNavStatusText = text;
    m_hasNavStatus = true;
    renderRobotTable();
}

void OperationsOverviewPage::refreshHealth()
{
    QVector<SystemHealthRow> rows;
    QString errorMessage;
    if (!m_service.fetchSystemHealth(rows, errorMessage)) {
        qWarning() << "시스템 헬스 조회 실패:" << errorMessage;
        return;
    }

    // rows[0]=ROS2 통신, rows[1]=DB 동기화 (DbOperationsOverviewService 참고)
    m_lastHealthDbRows = rows;
    renderHealthPanel();
}

void OperationsOverviewPage::renderHealthPanel()
{
    QVector<SystemHealthRow> rows;
    if (m_lastHealthDbRows.size() >= 1) {
        rows.push_back(m_lastHealthDbRows.at(0));
    }
    rows.push_back(nav2HealthRow(m_hasNav2Signal, m_nav2Connected, m_hasAmclPose));
    if (m_lastHealthDbRows.size() >= 2) {
        rows.push_back(m_lastHealthDbRows.at(1));
    }
    rows.push_back(liveHealthRow(QStringLiteral("카메라 연결"), m_hasCameraSignal, m_cameraConnected));

    clearLayout(m_healthLayout);
    for (const SystemHealthRow &row : rows) {
        m_healthLayout->addWidget(buildHealthRow(row));
    }
}

void OperationsOverviewPage::applyNav2Connection(bool connected)
{
    m_hasNav2Signal = true;
    m_nav2Connected = connected;
    if (!connected) {
        // nav2가 내려가면 이전에 알던 위치는 더 이상 유효하다고 볼 수 없다.
        m_hasAmclPose = false;
    }
    renderHealthPanel();
}

void OperationsOverviewPage::applyAmclPose(double x, double y, double yawRad)
{
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(yawRad);
    if (m_hasAmclPose) {
        return;
    }
    m_hasAmclPose = true;
    renderHealthPanel();
}

void OperationsOverviewPage::applyCameraConnection(bool connected)
{
    m_hasCameraSignal = true;
    m_cameraConnected = connected;
    renderHealthPanel();
}

void OperationsOverviewPage::refreshEvents()
{
    QVector<OperationsEventRow> rows;
    QString errorMessage;
    if (!m_service.fetchRecentEvents(5, rows, errorMessage)) {
        qWarning() << "최근 운영 이벤트 조회 실패:" << errorMessage;
        return;
    }

    clearLayout(m_eventsLayout);
    if (rows.isEmpty()) {
        m_eventsLayout->addWidget(makeLabel(QStringLiteral("최근 운영 이벤트가 없습니다."), QStringLiteral("metricCaption")));
        return;
    }
    for (const OperationsEventRow &event : rows) {
        m_eventsLayout->addWidget(buildEventRow(event));
    }
}
