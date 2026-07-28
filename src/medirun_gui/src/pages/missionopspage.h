#ifndef MISSIONOPSPAGE_H
#define MISSIONOPSPAGE_H

#include <QWidget>

class MissionOpsPage : public QWidget
{
    Q_OBJECT

public:
    explicit MissionOpsPage(QWidget *parent = nullptr);

signals:
    void missionCancelRequested();
    void returnHomeRequested();
    void remoteControlRequested();

private:
    QWidget *buildAnomalyCard();
    QWidget *buildWaitingQueue();
    QWidget *buildInProgressMission();
    QWidget *buildTodayHistory();
};

#endif // MISSIONOPSPAGE_H
