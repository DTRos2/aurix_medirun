#ifndef SYSTEMSETTINGSPAGE_H
#define SYSTEMSETTINGSPAGE_H

#include "../services/operationsoverviewservice.h"

#include <QVector>
#include <QWidget>

class QDateEdit;
class QVBoxLayout;
class QLabel;
class QTimer;
class QScrollArea;
class OperationsOverviewService;
class LiveSearchLineEdit;

class SystemSettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SystemSettingsPage(OperationsOverviewService &service, QWidget *parent = nullptr);

private:
    QWidget *buildFilterPanel();
    QWidget *buildResultsPanel();
    void scheduleDbRefresh();
    void refreshFromDb();
    void renderFilteredRows();

    OperationsOverviewService &m_service;
    QDateEdit *m_startDateEdit {nullptr};
    QDateEdit *m_endDateEdit {nullptr};
    LiveSearchLineEdit *m_keywordEdit {nullptr};
    QLabel *m_resultSummaryLabel {nullptr};
    QVBoxLayout *m_resultsListLayout {nullptr};
    QScrollArea *m_resultsScrollArea {nullptr};
    QTimer *m_dbRefreshDebounceTimer {nullptr};
    // 현재 날짜 범위로 DB에서 가져온 전체 행 — 검색어 필터링/하이라이트는 이
    // 캐시 위에서 순수 C++로만 처리해서 타이핑할 때마다 DB를 왕복하지 않는다
    // (날짜 범위를 바꿀 때만 다시 조회).
    QVector<OperationsEventRow> m_cachedRows;
};

#endif // SYSTEMSETTINGSPAGE_H
