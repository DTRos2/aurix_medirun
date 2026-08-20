#include "systemsettingspage.h"

#include "../theme/dashboardpalette.h"
#include "../widgets/badgelabel.h"
#include "../widgets/livesearchlineedit.h"

#include <QDate>
#include <QDateEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

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

void clearLayout(QVBoxLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

// 검색어와 일치하는 부분을 파란색 굵게 표시한 리치 텍스트를 만든다. text는
// 먼저 HTML 이스케이프한 뒤 그 위에 <span>을 씌우므로, 메시지 안에 "<"/"&"
// 같은 문자가 있어도 안전하다(태그로 오인되지 않음).
QString highlightKeyword(const QString &text, const QString &keyword)
{
    const QString escaped = text.toHtmlEscaped();
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty()) {
        return escaped;
    }

    const QRegularExpression pattern(
        QRegularExpression::escape(trimmedKeyword.toHtmlEscaped()), QRegularExpression::CaseInsensitiveOption);

    QString result;
    int lastEnd = 0;
    QRegularExpressionMatchIterator it = pattern.globalMatch(escaped);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result += escaped.mid(lastEnd, match.capturedStart() - lastEnd);
        result += QStringLiteral("<span style=\"color:%1; font-weight:700;\">%2</span>")
                       .arg(DashboardPalette::Cyan.name(), match.captured(0));
        lastEnd = match.capturedEnd();
    }
    result += escaped.mid(lastEnd);
    return result;
}

bool rowMatchesKeyword(const OperationsEventRow &row, const QString &keyword)
{
    if (keyword.isEmpty()) {
        return true;
    }
    return row.message.contains(keyword, Qt::CaseInsensitive);
}
} // namespace

SystemSettingsPage::SystemSettingsPage(OperationsOverviewService &service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
    // 날짜 범위가 바뀔 때만 DB를 다시 조회한다 — 검색어는 debounce 없이 이미
    // 받아온 캐시(m_cachedRows) 위에서 바로 필터링하므로 하이라이트가 즉시
    // 반응한다(이전엔 타이핑마다 DB 왕복 + debounce 때문에 느렸음).
    m_dbRefreshDebounceTimer = new QTimer(this);
    m_dbRefreshDebounceTimer->setSingleShot(true);
    m_dbRefreshDebounceTimer->setInterval(300);
    connect(m_dbRefreshDebounceTimer, &QTimer::timeout, this, &SystemSettingsPage::refreshFromDb);

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
    headerLayout->addWidget(makeLabel(QStringLiteral("이벤트 관리"), QStringLiteral("pageTitle")));
    headerLayout->addWidget(makeLabel(
        QStringLiteral("시스템 이벤트 로그(system_event)를 날짜·검색어로 조회합니다."), QStringLiteral("pageSubtitle")));
    layout->addWidget(headerPanel);

    layout->addWidget(buildFilterPanel());
    layout->addWidget(buildResultsPanel(), 1);

    refreshFromDb();
}

QWidget *SystemSettingsPage::buildFilterPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QHBoxLayout(panel);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    const QDate today = QDate::currentDate();

    layout->addWidget(makeLabel(QStringLiteral("기간"), QStringLiteral("metricCaption")));
    m_startDateEdit = new QDateEdit(today, panel);
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    layout->addWidget(m_startDateEdit);

    layout->addWidget(makeLabel(QStringLiteral("~"), QStringLiteral("metricCaption")));
    m_endDateEdit = new QDateEdit(today, panel);
    m_endDateEdit->setCalendarPopup(true);
    m_endDateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    layout->addWidget(m_endDateEdit);

    layout->addWidget(makeLabel(QStringLiteral("검색어"), QStringLiteral("metricCaption")));
    m_keywordEdit = new LiveSearchLineEdit(panel);
    m_keywordEdit->setPlaceholderText(QStringLiteral("이벤트 종류·메시지 검색 (비워두면 전체)"));
    layout->addWidget(m_keywordEdit, 1);

    // 날짜 변경 → DB 재조회(debounce). 검색어 변경 → 캐시에서 즉시 필터링(DB 안 감).
    connect(m_startDateEdit, &QDateEdit::dateChanged, this, &SystemSettingsPage::scheduleDbRefresh);
    connect(m_endDateEdit, &QDateEdit::dateChanged, this, &SystemSettingsPage::scheduleDbRefresh);
    // textChanged가 아니라 liveTextChanged: 한글 조합 중(preedit) 글자까지
    // 포함해서 즉시 반영하기 위함 — LiveSearchLineEdit 주석 참고.
    connect(m_keywordEdit, &LiveSearchLineEdit::liveTextChanged, this, &SystemSettingsPage::renderFilteredRows);

    return panel;
}

void SystemSettingsPage::scheduleDbRefresh()
{
    m_dbRefreshDebounceTimer->start();
}

QWidget *SystemSettingsPage::buildResultsPanel()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    m_resultSummaryLabel = makeLabel(QString(), QStringLiteral("panelTitle"));
    layout->addWidget(m_resultSummaryLabel);

    auto *resultsContainer = new QWidget;
    m_resultsListLayout = new QVBoxLayout(resultsContainer);
    m_resultsListLayout->setContentsMargins(0, 0, 0, 4);
    m_resultsListLayout->setSpacing(8);

    // 미션 관제 "오늘 처리 이력"/실시간 관제 "이벤트 스트림"과 동일한 패턴:
    // 조회 결과가 많아도 패널이 무한정 늘어나지 않게 스크롤 영역에 담는다.
    m_resultsScrollArea = new QScrollArea(panel);
    m_resultsScrollArea->setWidget(resultsContainer);
    m_resultsScrollArea->setWidgetResizable(true);
    m_resultsScrollArea->setFrameShape(QFrame::NoFrame);
    m_resultsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    layout->addWidget(m_resultsScrollArea, 1);

    return panel;
}

void SystemSettingsPage::refreshFromDb()
{
    const QDate startDate = m_startDateEdit->date();
    const QDate endDate = m_endDateEdit->date();
    if (startDate > endDate) {
        m_cachedRows.clear();
        m_resultSummaryLabel->setText(QStringLiteral("조회 실패 · 시작일이 종료일보다 늦습니다."));
        clearLayout(m_resultsListLayout);
        m_resultsListLayout->addStretch();
        return;
    }

    QString errorMessage;
    // 검색어는 여기서 안 넘긴다 — 날짜 범위 전체를 캐시에 받아두고, 검색어
    // 필터링은 renderFilteredRows()가 캐시 위에서 즉시 처리한다.
    if (!m_service.fetchSystemEvents(startDate, endDate, QString(), m_cachedRows, errorMessage)) {
        m_cachedRows.clear();
        m_resultSummaryLabel->setText(QStringLiteral("조회 실패 · %1").arg(errorMessage));
        clearLayout(m_resultsListLayout);
        m_resultsListLayout->addStretch();
        return;
    }

    std::sort(m_cachedRows.begin(), m_cachedRows.end(), [](const OperationsEventRow &a, const OperationsEventRow &b) {
        return a.occurredAt > b.occurredAt;
    });

    renderFilteredRows();
}

void SystemSettingsPage::renderFilteredRows()
{
    if (m_resultsListLayout == nullptr) {
        return;
    }

    // setUpdatesEnabled(false)로 감싸서, 아래에서 라벨을 통째로 지웠다가 다시
    // 만드는 동안 화면에 중간 상태가 그려지지 않게 막는다. 다시 true로
    // 돌리는 순간 Qt가 자동으로 위젯 전체에 대한 완전한 리페인트를 예약하므로
    // (이전에 시도했던 개별 update()/repaint() 수동 호출 조합과 달리) 새로
    // 만든 리치텍스트 라벨이 클릭 등 다음 이벤트 없이도 바로 완전히
    // 그려진다.
    m_resultsScrollArea->setUpdatesEnabled(false);

    clearLayout(m_resultsListLayout);

    const QString keyword = m_keywordEdit->liveText().trimmed();
    QVector<OperationsEventRow> filtered;
    filtered.reserve(m_cachedRows.size());
    for (const OperationsEventRow &row : std::as_const(m_cachedRows)) {
        if (rowMatchesKeyword(row, keyword)) {
            filtered.append(row);
        }
    }

    m_resultSummaryLabel->setText(QStringLiteral("조회 결과 · %1건").arg(filtered.size()));

    if (filtered.isEmpty()) {
        m_resultsListLayout->addWidget(makeLabel(QStringLiteral("조건에 맞는 이벤트가 없습니다."), QStringLiteral("metricCaption")));
        m_resultsListLayout->addStretch();
        m_resultsScrollArea->setUpdatesEnabled(true);
        return;
    }

    for (const OperationsEventRow &row : std::as_const(filtered)) {
        auto *rowWidget = new QWidget;
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);

        auto *timeLabel = makeLabel(row.occurredAt.toString(QStringLiteral("MM-dd HH:mm:ss")), QStringLiteral("metricCaption"));
        timeLabel->setFixedWidth(90);
        rowLayout->addWidget(timeLabel);

        rowLayout->addWidget(makeBadgeLabel(row.severityLabel, row.kind, rowWidget));

        auto *messageLabel = makeLabel(QString());
        messageLabel->setTextFormat(Qt::RichText);
        messageLabel->setText(highlightKeyword(row.message, keyword));
        rowLayout->addWidget(messageLabel, 1);

        m_resultsListLayout->addWidget(rowWidget);
    }
    m_resultsListLayout->addStretch();

    if (QWidget *resultsContainer = m_resultsListLayout->parentWidget()) {
        if (QLayout *containerLayout = resultsContainer->layout()) {
            containerLayout->activate();
        }
        resultsContainer->adjustSize();
    }
    m_resultsScrollArea->setUpdatesEnabled(true);
}
