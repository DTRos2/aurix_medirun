#ifndef ROBOTMANAGEMENTPAGE_H
#define ROBOTMANAGEMENTPAGE_H

#include <QWidget>

class RobotService;
class QLabel;
class QLineEdit;
class QTableWidget;

class RobotManagementPage : public QWidget
{
    Q_OBJECT

public:
    explicit RobotManagementPage(RobotService &robotService, QWidget *parent = nullptr);

private:
    QWidget *buildRegisterForm();
    void refreshRobotTable();
    void handleRegisterRobot();
    void handleDeregisterRobot(int robotId, const QString &name);

    RobotService &m_robotService;
    QLineEdit *m_nameEdit;
    QLineEdit *m_modelEdit;
    QLabel *m_statusLabel;
    QTableWidget *m_robotTable;
};

#endif // ROBOTMANAGEMENTPAGE_H
