#include "logindialog.h"

#include "../services/authservice.h"

#include <QFile>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMovie>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QLabel *createBrandLogoLabel(QWidget *parent, const QSize &targetSize)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("brandLogo"));
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(targetSize);
    label->setMaximumSize(targetSize);

    const QString logoPath = QStringLiteral(":/images/aurix_logo_01.png");
    const QPixmap logo(logoPath);
    if (logo.isNull()) {
        qWarning() << "Login logo load failed:" << logoPath << "exists:" << QFile::exists(logoPath);
        label->setText(QStringLiteral("A"));
        return label;
    }

    qInfo() << "Login logo load success:" << logoPath;
    label->setPixmap(logo.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    return label;
}

QLabel *createPreviewPanelImageLabel(QWidget *parent, const QSize &targetSize)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QStringLiteral("loginPreviewImage"));
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(targetSize);
    label->setScaledContents(false);

    const QString animationPath = QStringLiteral(":/assets/AURIX_MediRun_Animated_Bright_Logo.gif");
    auto *movie = new QMovie(animationPath, QByteArray(), label);
    if (!movie->isValid()) {
        qWarning() << "Login preview GIF load failed:" << animationPath << "exists:" << QFile::exists(animationPath);
        delete movie;
        label->setText(QStringLiteral("PREVIEW"));
        return label;
    }

    movie->setCacheMode(QMovie::CacheAll);
    movie->setScaledSize(targetSize);
    label->setMovie(movie);
    movie->start();
    return label;
}
}

LoginDialog::LoginDialog(AuthService &authService, bool mockMode, QWidget *parent)
    : QDialog(parent)
    , m_authService(authService)
    , m_usernameEdit(nullptr)
    , m_passwordEdit(nullptr)
    , m_statusLabel(nullptr)
    , m_togglePasswordButton(nullptr)
    , m_loginCard(nullptr)
{
    buildUi(mockMode);
}

AppSession LoginDialog::session() const
{
    return m_session;
}

void LoginDialog::attemptLogin()
{
    QString errorMessage;
    AppSession session;
    const bool ok = m_authService.authenticate(
        m_usernameEdit->text().trimmed(),
        m_passwordEdit->text(),
        selectedRole(),
        session,
        errorMessage);

    if (!ok) {
        m_statusLabel->setText(errorMessage);
        return;
    }

    m_session = session;
    accept();
}

void LoginDialog::fillStaffCredentials()
{
    m_usernameEdit->setText(QStringLiteral("user"));
    m_passwordEdit->setText(QStringLiteral("user123"));
    m_statusLabel->setText(QStringLiteral("USER Mock 계정이 선택되었습니다."));
}

void LoginDialog::fillAdminCredentials()
{
    m_usernameEdit->setText(QStringLiteral("admin"));
    m_passwordEdit->setText(QStringLiteral("admin123"));
    m_statusLabel->setText(QStringLiteral("ADMIN Mock 계정이 선택되었습니다."));
}

void LoginDialog::togglePasswordVisibility()
{
    const bool hidden = m_passwordEdit->echoMode() == QLineEdit::Password;
    m_passwordEdit->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    m_togglePasswordButton->setText(hidden ? QStringLiteral("숨기기") : QStringLiteral("보기"));
}

void LoginDialog::buildUi(bool mockMode)
{
    setWindowTitle(QStringLiteral("AURIX MediRun 로그인"));
    setModal(true);
    resize(1440, 920);
    setMinimumSize(1400, 880);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *visual = new QFrame(this);
    visual->setObjectName(QStringLiteral("loginPreviewPanel"));
    visual->setMinimumWidth(780);
    auto *visualLayout = new QVBoxLayout(visual);
    visualLayout->setContentsMargins(6, 6, 6, 6);
    visualLayout->setSpacing(0);
    auto *previewImageLabel = createPreviewPanelImageLabel(visual, QSize(860, 980));
    visualLayout->addWidget(previewImageLabel, 1, Qt::AlignCenter);

    auto *formArea = new QFrame(this);
    formArea->setObjectName(QStringLiteral("loginFormPanel"));
    auto *formAreaLayout = new QVBoxLayout(formArea);
    formAreaLayout->setContentsMargins(70, 50, 70, 50);
    formAreaLayout->addStretch();

    m_loginCard = new QFrame(formArea);
    m_loginCard->setObjectName(QStringLiteral("loginCard"));
    auto *shadow = new QGraphicsDropShadowEffect(m_loginCard);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 16);
    shadow->setColor(QColor(0, 0, 0, 140));
    m_loginCard->setGraphicsEffect(shadow);

    auto *cardLayout = new QVBoxLayout(m_loginCard);
    cardLayout->setContentsMargins(34, 30, 34, 30);
    cardLayout->setSpacing(14);

    auto *kicker = new QLabel(QStringLiteral("OPERATOR SIGN IN"), m_loginCard);
    kicker->setObjectName(QStringLiteral("loginKicker"));
    cardLayout->addWidget(kicker);
    auto *titleLabel = new QLabel(QStringLiteral("운영 시스템 로그인"), m_loginCard);
    titleLabel->setObjectName(QStringLiteral("loginTitle"));
    cardLayout->addWidget(titleLabel);
    auto *subtitleLabel = new QLabel(QStringLiteral("허가된 계정으로 안전하게 접속하세요."), m_loginCard);
    subtitleLabel->setObjectName(QStringLiteral("loginSubtitle"));
    cardLayout->addWidget(subtitleLabel);
    cardLayout->addSpacing(4);

    auto *usernameLabel = new QLabel(QStringLiteral("아이디"), m_loginCard);
    usernameLabel->setObjectName(QStringLiteral("fieldLabel"));
    cardLayout->addWidget(usernameLabel);

    m_usernameEdit = new QLineEdit(m_loginCard);
    m_usernameEdit->setPlaceholderText(QStringLiteral("이메일 또는 아이디"));
    cardLayout->addWidget(m_usernameEdit);

    auto *passwordLabel = new QLabel(QStringLiteral("비밀번호"), m_loginCard);
    passwordLabel->setObjectName(QStringLiteral("fieldLabel"));
    cardLayout->addWidget(passwordLabel);

    auto *passwordRow = new QWidget(m_loginCard);
    auto *passwordLayout = new QHBoxLayout(passwordRow);
    passwordLayout->setContentsMargins(0, 0, 0, 0);
    passwordLayout->setSpacing(8);

    m_passwordEdit = new QLineEdit(passwordRow);
    m_passwordEdit->setPlaceholderText(QStringLiteral("비밀번호"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    passwordLayout->addWidget(m_passwordEdit, 1);

    m_togglePasswordButton = new QPushButton(QStringLiteral("보기"), passwordRow);
    m_togglePasswordButton->setObjectName(QStringLiteral("inlineGhostButton"));
    passwordLayout->addWidget(m_togglePasswordButton);
    cardLayout->addWidget(passwordRow);

    if (mockMode) {
        auto *mockButtons = new QHBoxLayout();
        auto *staffButton = new QPushButton(QStringLiteral("USER Mock"), m_loginCard);
        auto *adminButton = new QPushButton(QStringLiteral("ADMIN Mock"), m_loginCard);
        staffButton->setObjectName(QStringLiteral("inlineGhostButton"));
        adminButton->setObjectName(QStringLiteral("inlineGhostButton"));
        mockButtons->addWidget(staffButton);
        mockButtons->addWidget(adminButton);
        cardLayout->addLayout(mockButtons);
        connect(staffButton, &QPushButton::clicked, this, &LoginDialog::fillStaffCredentials);
        connect(adminButton, &QPushButton::clicked, this, &LoginDialog::fillAdminCredentials);
    }

    m_statusLabel = new QLabel(QStringLiteral("로그인 후 운영 대시보드가 열립니다."), m_loginCard);
    m_statusLabel->setObjectName(QStringLiteral("loginStatus"));
    m_statusLabel->setWordWrap(true);
    cardLayout->addWidget(m_statusLabel);

    auto *loginButton = new QPushButton(QStringLiteral("대시보드 시작 →"), m_loginCard);
    loginButton->setObjectName(QStringLiteral("primaryCtaButton"));
    cardLayout->addWidget(loginButton);
    auto *demoLabel = new QLabel(QStringLiteral("기본 계정 • admin / admin123 | user / user123"), m_loginCard);
    demoLabel->setObjectName(QStringLiteral("demoHint"));
    cardLayout->addWidget(demoLabel);

    formAreaLayout->addWidget(m_loginCard);
    formAreaLayout->addStretch();

    rootLayout->addWidget(visual, 12);
    rootLayout->addWidget(formArea, 10);

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::attemptLogin);
    connect(m_usernameEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptLogin);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::attemptLogin);
    connect(m_togglePasswordButton, &QPushButton::clicked, this, &LoginDialog::togglePasswordVisibility);
    QTimer::singleShot(60, this, [this]() {
        const QPoint origin = m_loginCard->pos();
        m_loginCard->move(origin + QPoint(0, 22));
        auto *animation = new QPropertyAnimation(m_loginCard, "pos", m_loginCard);
        animation->setDuration(420);
        animation->setStartValue(m_loginCard->pos());
        animation->setEndValue(origin);
        animation->setEasingCurve(QEasingCurve::OutBack);
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

UserRole LoginDialog::selectedRole() const
{
    return UserRole::User;
}
