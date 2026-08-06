#ifndef MISSIONOPSPAGE_H
#define MISSIONOPSPAGE_H

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QPushButton;
class MissionController;

class MissionOpsPage : public QWidget
{
    Q_OBJECT

public:
    explicit MissionOpsPage(QWidget *parent = nullptr);
    void setMissionController(MissionController *controller);

signals:
    void retryRequested();
    void missionCancelRequested();
    void returnHomeRequested();
    void queueCancelRequested(const QString &queueId);
    void queueDivertRequested(const QString &queueId);
    void remoteControlRequested();

private:
    QWidget *buildAnomalyCard();
    QWidget *buildWaitingQueue();
    QWidget *buildInProgressMission();
    QWidget *buildTodayHistory();
    void refreshQueue();
    void refreshMission();
    void refreshHistory();
    void refreshAnomaly();
    void refreshAll();

    MissionController *m_controller {nullptr};
    QLabel *m_queueTitleLabel {nullptr};
    QLabel *m_anomalyTitleLabel {nullptr};
    QLabel *m_anomalyMetaLabel {nullptr};
    QLabel *m_anomalyDetailLabel {nullptr};
    QPushButton *m_retryButton {nullptr};
    QVBoxLayout *m_queueListLayout {nullptr};
    QLabel *m_missionTitleLabel {nullptr};
    QLabel *m_missionStateLabel {nullptr};
    QWidget *m_missionProgressBar {nullptr};
    QLabel *m_missionDetailLabel {nullptr};
    QLabel *m_missionMetaLabel {nullptr};
    QVBoxLayout *m_historyListLayout {nullptr};
};

#endif // MISSIONOPSPAGE_H
