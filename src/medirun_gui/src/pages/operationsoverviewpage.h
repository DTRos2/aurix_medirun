#ifndef OPERATIONSOVERVIEWPAGE_H
#define OPERATIONSOVERVIEWPAGE_H

#include <QWidget>

class QLabel;
class QTableWidget;

class OperationsOverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OperationsOverviewPage(QWidget *parent = nullptr);

private:
    QWidget *buildHeader();
    QWidget *buildMetricCard(const QString &title, const QString &value, const QString &caption, const QString &kind);
    QWidget *buildRobotStatusPanel();
    QWidget *buildHealthPanel();
    QWidget *buildEventsPanel();
};

#endif // OPERATIONSOVERVIEWPAGE_H
