#include "usermanagementpage.h"

#include "../services/authservice.h"

#include <QAbstractItemView>
#include <QComboBox>
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

UserManagementPage::UserManagementPage(AuthService &authService, QWidget *parent)
    : QWidget(parent)
    , m_authService(authService)
    , m_emailEdit(nullptr)
    , m_passwordEdit(nullptr)
    , m_roleCombo(nullptr)
    , m_statusLabel(nullptr)
    , m_userTable(nullptr)
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
    headerLayout->addWidget(makeLabel(QStringLiteral("사용자 관리"), QStringLiteral("pageTitle")));
    headerLayout->addWidget(makeLabel(
        QStringLiteral("이메일 또는 아이디, 비밀번호, 권한을 등록하고 즉시 로그인 가능한 계정을 생성합니다."),
        QStringLiteral("pageSubtitle")));
    layout->addWidget(headerPanel);

    auto *bodyRow = new QHBoxLayout;
    bodyRow->setSpacing(14);
    bodyRow->addWidget(buildCreateForm(), 4);

    auto *listPanel = panelFrame(QStringLiteral("contentPanel"));
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(18, 18, 18, 18);
    listLayout->setSpacing(12);
    listLayout->addWidget(makeLabel(QStringLiteral("등록된 사용자"), QStringLiteral("panelTitle")));
    m_userTable = new QTableWidget(listPanel);
    m_userTable->setColumnCount(3);
    m_userTable->setHorizontalHeaderLabels({QStringLiteral("이메일/아이디"), QStringLiteral("권한"), QStringLiteral("")});
    m_userTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    // ResizeToContents는 setCellWidget()으로 넣은 버튼의 sizeHint를 제대로
    // 못 읽어서 칸이 버튼 글자보다 좁아지는 경우가 있었다 — 고정 폭으로 넉넉히 준다.
    m_userTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_userTable->setColumnWidth(2, 104);
    m_userTable->verticalHeader()->setVisible(false);
    // 기본 행 높이가 삭제 버튼(여백 포함 32px)보다 작아서, 버튼을 여백
    // 컨테이너로 감싸자 오히려 그 안에서 짜부라져 글자가 안 보였다 —
    // 버튼이 여유 있게 들어갈 만큼 행 높이를 명시적으로 키운다.
    m_userTable->verticalHeader()->setDefaultSectionSize(42);
    m_userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    listLayout->addWidget(m_userTable, 1);
    bodyRow->addWidget(listPanel, 6);

    layout->addLayout(bodyRow, 1);
    refreshUserTable();
}

QWidget *UserManagementPage::buildCreateForm()
{
    auto *panel = panelFrame(QStringLiteral("contentPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);
    layout->addWidget(makeLabel(QStringLiteral("계정 추가"), QStringLiteral("panelTitle")));
    layout->addWidget(makeLabel(QStringLiteral("관리자가 추가한 계정은 즉시 로그인에 사용할 수 있습니다."), QStringLiteral("pageSubtitle")));

    layout->addWidget(makeLabel(QStringLiteral("이메일 또는 아이디"), QStringLiteral("fieldLabel")));
    m_emailEdit = new QLineEdit(panel);
    m_emailEdit->setPlaceholderText(QStringLiteral("operator@aurix.local"));
    layout->addWidget(m_emailEdit);

    layout->addWidget(makeLabel(QStringLiteral("비밀번호"), QStringLiteral("fieldLabel")));
    m_passwordEdit = new QLineEdit(panel);
    m_passwordEdit->setPlaceholderText(QStringLiteral("8자 이상"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_passwordEdit);

    layout->addWidget(makeLabel(QStringLiteral("권한"), QStringLiteral("fieldLabel")));
    m_roleCombo = new QComboBox(panel);
    m_roleCombo->addItem(QStringLiteral("USER"), static_cast<int>(UserRole::Staff));
    m_roleCombo->addItem(QStringLiteral("ADMIN"), static_cast<int>(UserRole::Admin));
    layout->addWidget(m_roleCombo);

    auto *createButton = new QPushButton(QStringLiteral("사용자 추가"));
    createButton->setObjectName(QStringLiteral("primaryCtaButton"));
    layout->addWidget(createButton);

    m_statusLabel = makeLabel(QStringLiteral("기본 계정 admin/user 외에도 신규 계정을 등록할 수 있습니다."), QStringLiteral("pageSubtitle"));
    layout->addWidget(m_statusLabel);
    layout->addStretch();

    connect(createButton, &QPushButton::clicked, this, &UserManagementPage::handleCreateUser);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &UserManagementPage::handleCreateUser);
    return panel;
}

void UserManagementPage::refreshUserTable()
{
    const QVector<AuthUserRecord> users = m_authService.users();
    m_userTable->setRowCount(users.size());
    for (int row = 0; row < users.size(); ++row) {
        const AuthUserRecord &user = users.at(row);
        m_userTable->setItem(row, 0, new QTableWidgetItem(user.loginId));
        m_userTable->setItem(row, 1, new QTableWidgetItem(roleDisplayName(user.role)));

        // auto *deleteButton = new QPushButton(QStringLiteral("삭제"));
        // deleteButton->setObjectName(QStringLiteral("tableRowDangerButton"));
        auto *deleteButton = new QPushButton(QStringLiteral("삭제"));
        deleteButton->setObjectName(QStringLiteral("tableRowDangerButton"));
        deleteButton->setFixedHeight(26);                  // 행 높이보다 확실히 작게
        deleteButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        deleteButton->setMinimumWidth(64);
        connect(deleteButton, &QPushButton::clicked, this, [this, loginId = user.loginId]() {
            handleDeleteUser(loginId);
        });
        // setCellWidget()은 위젯을 셀 크기 그대로 채워버려서, 버튼을 아무리
        // 작게 잡아도 행 경계까지 꽉 차 옆/위아래 행과 거의 붙어 보인다 —
        // 여백을 가진 컨테이너로 한 번 감싸서 셀 안에 숨 쉴 틈을 준다.
        auto *cell = new QWidget(m_userTable);
        auto *cellLayout = new QHBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(0);
        cellLayout->addWidget(deleteButton);
        // cellLayout->addWidget(deleteButton, 0, Qt::AlignCenter);
        m_userTable->setCellWidget(row, 2, cell);
    }
}

void UserManagementPage::handleCreateUser()
{
    QString errorMessage;
    const UserRole role = static_cast<UserRole>(m_roleCombo->currentData().toInt());
    const bool ok = m_authService.createUser(m_emailEdit->text(), m_passwordEdit->text(), role, errorMessage);

    if (!ok) {
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_statusLabel->setText(
        QStringLiteral("%1 계정이 생성되었습니다. 이제 해당 계정으로 로그인할 수 있습니다.").arg(m_emailEdit->text().trimmed()));
    m_emailEdit->clear();
    m_passwordEdit->clear();
    refreshUserTable();
}

void UserManagementPage::handleDeleteUser(const QString &loginId)
{
    const auto confirm = QMessageBox::question(
        this,
        QStringLiteral("계정 삭제"),
        QStringLiteral("%1 계정을 삭제하시겠습니까?").arg(loginId));
    if (confirm != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    const bool ok = m_authService.deleteUser(loginId, errorMessage);
    if (!ok) {
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_statusLabel->setText(QStringLiteral("%1 계정이 삭제되었습니다.").arg(loginId));
    refreshUserTable();
}
