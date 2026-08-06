#include "robotmanagementpage.h"

#include "../services/robotservice.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

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
} // namespace

RobotManagementPage::RobotManagementPage(RobotService &robotService, QWidget *parent)
    : QWidget(parent)
    , m_robotService(robotService)
    , m_nameEdit(nullptr)
    , m_modelEdit(nullptr)
    , m_statusLabel(nullptr)
    , m_robotTable(nullptr)
{
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
    headerLayout->addWidget(makeLabel(QStringLiteral("로봇 관리"), QStringLiteral("pageTitle")));
    headerLayout->addWidget(makeLabel(
        QStringLiteral("현재 운영 중인 로봇을 등록합니다. 연결 상태·배터리는 운영 현황 화면에 실시간으로 표시됩니다."),
        QStringLiteral("pageSubtitle")));
    layout->addWidget(headerPanel);

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(14);
    bodyRow->addWidget(buildRegisterForm(), 4);

    auto *listPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(18, 18, 18, 18);
    listLayout->setSpacing(12);
    listLayout->addWidget(makeLabel(QStringLiteral("등록된 로봇"), QStringLiteral("panelTitle")));
    m_robotTable = new QTableWidget(listPanel);
    m_robotTable->setColumnCount(3);
    m_robotTable->setHorizontalHeaderLabels({QStringLiteral("이름"), QStringLiteral("모델"), QStringLiteral("")});
    m_robotTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_robotTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_robotTable->verticalHeader()->setVisible(false);
    m_robotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_robotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_robotTable->setSelectionMode(QAbstractItemView::SingleSelection);
    listLayout->addWidget(m_robotTable, 1);
    bodyRow->addWidget(listPanel, 6);

    layout->addLayout(bodyRow, 1);
    refreshRobotTable();
}

QWidget *RobotManagementPage::buildRegisterForm()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("로봇 등록"), QStringLiteral("panelTitle")));
    layout->addWidget(makeLabel(
        QStringLiteral("SSH로 bringup을 실행 중인 로봇의 이름과 모델을 등록하세요."), QStringLiteral("pageSubtitle")));

    layout->addWidget(makeLabel(QStringLiteral("로봇 이름"), QStringLiteral("fieldLabel")));
    m_nameEdit = new QLineEdit(panel);
    m_nameEdit->setPlaceholderText(QStringLiteral("burger01"));
    layout->addWidget(m_nameEdit);

    layout->addWidget(makeLabel(QStringLiteral("모델"), QStringLiteral("fieldLabel")));
    m_modelEdit = new QLineEdit(panel);
    m_modelEdit->setPlaceholderText(QStringLiteral("turtlebot3_burger"));
    layout->addWidget(m_modelEdit);

    auto *registerButton = new QPushButton(QStringLiteral("로봇 등록"));
    registerButton->setObjectName(QStringLiteral("primaryCtaButton"));
    layout->addWidget(registerButton);

    m_statusLabel = makeLabel(QString(), QStringLiteral("pageSubtitle"));
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    connect(registerButton, &QPushButton::clicked, this, &RobotManagementPage::handleRegisterRobot);
    connect(m_modelEdit, &QLineEdit::returnPressed, this, &RobotManagementPage::handleRegisterRobot);
    return panel;
}

void RobotManagementPage::refreshRobotTable()
{
    const QVector<RobotRecord> robots = m_robotService.listRobots(false);
    m_robotTable->setRowCount(robots.size());
    for (int row = 0; row < robots.size(); ++row) {
        const RobotRecord &robot = robots.at(row);
        m_robotTable->setItem(row, 0, new QTableWidgetItem(robot.name));
        m_robotTable->setItem(row, 1, new QTableWidgetItem(robot.model));

        auto *deleteButton = new QPushButton(QStringLiteral("등록 해제"), m_robotTable);
        deleteButton->setObjectName(QStringLiteral("dangerFilledButton"));
        connect(deleteButton, &QPushButton::clicked, this, [this, id = robot.id, name = robot.name]() {
            handleDeregisterRobot(id, name);
        });
        m_robotTable->setCellWidget(row, 2, deleteButton);
    }
}

void RobotManagementPage::handleRegisterRobot()
{
    QString errorMessage;
    const bool ok = m_robotService.registerRobot(m_nameEdit->text(), m_modelEdit->text(), errorMessage);

    if (!ok) {
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_statusLabel->setText(QStringLiteral("%1 로봇이 등록되었습니다.").arg(m_nameEdit->text().trimmed()));
    m_nameEdit->clear();
    m_modelEdit->clear();
    refreshRobotTable();
}

void RobotManagementPage::handleDeregisterRobot(int robotId, const QString &name)
{
    const auto confirm = QMessageBox::question(
        this, QStringLiteral("로봇 등록 해제"), QStringLiteral("%1 로봇을 등록 해제하시겠습니까?").arg(name));
    if (confirm != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    const bool ok = m_robotService.deregisterRobot(robotId, errorMessage);
    if (!ok) {
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_statusLabel->setText(QStringLiteral("%1 로봇이 등록 해제되었습니다.").arg(name));
    refreshRobotTable();
}
