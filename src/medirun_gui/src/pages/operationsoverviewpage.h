#ifndef OPERATIONSOVERVIEWPAGE_H
#define OPERATIONSOVERVIEWPAGE_H

#include "../models/telemetrymodel.h"
#include "../services/operationsoverviewservice.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QTableWidget;
class QVBoxLayout;
class QTimer;
class OperationsOverviewService;

class OperationsOverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OperationsOverviewPage(OperationsOverviewService &service, QWidget *parent = nullptr);

    // ROS 배터리 텔레메트리 스냅샷을 반영해 로봇 상태 표의 배터리 칸을
    // DB 재조회 없이 즉시 갱신한다. driving_record가 비어있는 한 배터리
    // 연속 수치는 DB에 남지 않으므로 이 라이브 오버레이가 유일한 소스다.
    void applyTelemetrySnapshot(const TelemetrySnapshot &snapshot);

    // Nav2/카메라 연결 여부는 DB에 기록되지 않는(연결될 대상 자체가 스키마
    // enum에 없는) 값이라 NavigationService/CameraFeedProvider의 실시간
    // 신호를 그대로 시스템 헬스 패널에 반영한다.
    void applyNav2Connection(bool connected);
    // AMCL은 초기 위치를 받기 전엔 amcl_pose를 발행하지 않으므로, 이 신호를
    // 한 번이라도 받았는지가 "초기 위치 설정됨"의 대리 신호가 된다.
    // nav2Connected(액션 서버 ACTIVE)만으로는 실제 주행 가능 여부를 보장하지
    // 않는다 — 위치를 모르면 goal은 받아도 주행이 실패/오동작할 수 있다.
    void applyAmclPose(double x, double y, double yawRad);
    void applyCameraConnection(bool connected);
    // 웨이포인트 페이지의 "주행" 버튼은 Nav2 액션을 직접 호출할 뿐 mission/
    // robot_session에는 아무것도 기록하지 않아서, DB 기준 로봇 상태는 실제로
    // 주행 중이어도 항상 "대기"로 보인다. NavigationService::statusChanged가
    // 이미 사람이 읽을 수 있는 실시간 문구("목적지로 이동 중...", "경로 주행
    // 중 (0/3)" 등)를 내보내므로 그걸 그대로 로봇 상태 칸에 덮어씌운다.
    void applyNavStatus(const QString &text);

private:
    struct MetricCardWidgets
    {
        QLabel *valueLabel {nullptr};
        QLabel *captionLabel {nullptr};
    };

    QWidget *buildHeader();
    QWidget *buildMetricCard(const QString &title, MetricCardWidgets &widgetsOut);
    QWidget *buildRobotStatusPanel();
    QWidget *buildHealthPanel();
    QWidget *buildEventsPanel();

    void refresh();
    void refreshSummary();
    void refreshRobotStatus();
    void refreshHealth();
    void refreshEvents();
    void renderRobotTable();
    void renderHealthPanel();

    OperationsOverviewService &m_service;

    QVector<MetricCardWidgets> m_metricCards;
    QTableWidget *m_robotTable {nullptr};
    QVBoxLayout *m_healthLayout {nullptr};
    QVBoxLayout *m_eventsLayout {nullptr};
    QTimer *m_refreshTimer {nullptr};

    QVector<RobotStatusRow> m_lastRobotRows;
    TelemetrySnapshot m_latestSnapshot;
    bool m_hasSnapshot {false};
    bool m_hasNavStatus {false};
    QString m_liveNavStatusText;

    // DB 기반 2행(ROS2 통신/DB 동기화)만 여기 담고, Nav2/카메라는 아래 라이브
    // 상태로 렌더 시점에 합쳐 4행을 만든다.
    QVector<SystemHealthRow> m_lastHealthDbRows;
    // renderHealthPanel()이 실제로 화면에 그린 마지막 행 값 — 값이 그대로면
    // 위젯을 다시 만들지 않고 건너뛰어 깜빡임을 막는다.
    QVector<SystemHealthRow> m_lastRenderedHealthRows;
    bool m_hasNav2Signal {false};
    bool m_nav2Connected {false};
    bool m_hasAmclPose {false};
    bool m_hasCameraSignal {false};
    bool m_cameraConnected {false};
};

#endif // OPERATIONSOVERVIEWPAGE_H
