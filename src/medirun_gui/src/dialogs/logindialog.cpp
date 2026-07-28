#include "logindialog.h"

#include "../services/authservice.h"

#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QPixmap>
#include <QTimer>
#include <QToolButton>
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

QIcon createEyeIcon(bool crossed)
{
    const QColor strokeColor(QStringLiteral("#9fb7c4"));

    const int size = 32;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(strokeColor, 2.1));
    painter.setBrush(Qt::NoBrush);

    QPainterPath eyePath;
    eyePath.moveTo(4, 16);
    eyePath.quadTo(16, 4, 28, 16);
    eyePath.quadTo(16, 28, 4, 16);
    painter.drawPath(eyePath);
    painter.drawEllipse(QPointF(16, 16), 4.2, 4.2);

    if (crossed) {
        painter.drawLine(QPointF(5, 5), QPointF(27, 27));
    }

    painter.end();
    return QIcon(pixmap);
}
}

LoginDialog::LoginDialog(AuthService &authService, bool mockMode, QWidget *parent)
    : QDialog(parent)
    , m_authService(authService)
    , m_usernameEdit(nullptr)
    , m_passwordEdit(nullptr)
    , m_statusLabel(nullptr)
    , m_errorLabel(nullptr)
    , m_passwordVisibilityButton(nullptr)
    , m_loginButton(nullptr)
    , m_loginCard(nullptr)
    , m_failedAttempts(0)
{
    buildUi(mockMode);
}

AppSession LoginDialog::session() const
{
    return m_session;
}

void LoginDialog::attemptLogin()
{
    m_errorLabel->clear();
    setFormBusy(true);

    QString errorMessage;
    AppSession session;
    const bool ok = m_authService.authenticate(
        m_usernameEdit->text().trimmed(),
        m_passwordEdit->text(),
        selectedRole(),
        session,
        errorMessage);

    if (!ok) {
        ++m_failedAttempts;
        qWarning().noquote() << QStringLiteral("[auth] 로그인 실패 (%1/%2회) · 계정: %3")
                                     .arg(m_failedAttempts)
                                     .arg(kMaxLoginAttempts)
                                     .arg(m_usernameEdit->text().trimmed());
        m_errorLabel->setText(QStringLiteral("⚠ %1 (%2/%3)")
                                   .arg(errorMessage)
                                   .arg(m_failedAttempts)
                                   .arg(kMaxLoginAttempts));
        setFormBusy(false);
        return;
    }

    m_session = session;
    accept();
}

void LoginDialog::setFormBusy(bool busy)
{
    m_usernameEdit->setEnabled(!busy);
    m_passwordEdit->setEnabled(!busy);
    m_passwordVisibilityButton->setEnabled(!busy);
    m_loginButton->setEnabled(!busy);
    m_loginButton->setText(busy ? QStringLiteral("확인 중…") : QStringLiteral("대시보드 시작 →"));
}

void LoginDialog::fillStaffCredentials()
{
    m_usernameEdit->setText(QStringLiteral("user"));
    m_passwordEdit->setText(QStringLiteral("user123"));
    m_statusLabel->setText(QStringLiteral("일반 사용자 Mock 계정이 선택되었습니다."));
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
    m_passwordVisibilityButton->setIcon(createEyeIcon(hidden));
    m_passwordVisibilityButton->setToolTip(hidden ? QStringLiteral("비밀번호 숨기기") : QStringLiteral("비밀번호 보기"));
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_passwordEdit && event->type() == QEvent::Resize) {
        repositionPasswordVisibilityButton();
    }
    return QDialog::eventFilter(watched, event);
}

void LoginDialog::repositionPasswordVisibilityButton()
{
    const int x = m_passwordEdit->width() - m_passwordVisibilityButton->width() - 8;
    const int y = (m_passwordEdit->height() - m_passwordVisibilityButton->height()) / 2;
    m_passwordVisibilityButton->move(x, y);
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

    m_passwordEdit = new QLineEdit(m_loginCard);
    m_passwordEdit->setPlaceholderText(QStringLiteral("비밀번호"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setTextMargins(0, 0, 34, 0);
    cardLayout->addWidget(m_passwordEdit);

    m_passwordVisibilityButton = new QToolButton(m_passwordEdit);
    m_passwordVisibilityButton->setObjectName(QStringLiteral("passwordVisibilityButton"));
    m_passwordVisibilityButton->setCursor(Qt::PointingHandCursor);
    m_passwordVisibilityButton->setFocusPolicy(Qt::NoFocus);
    m_passwordVisibilityButton->setFixedSize(30, 30);
    m_passwordVisibilityButton->setIconSize(QSize(22, 22));
    m_passwordVisibilityButton->setIcon(createEyeIcon(false));
    m_passwordVisibilityButton->setToolTip(QStringLiteral("비밀번호 보기"));
    m_passwordEdit->installEventFilter(this);

    m_errorLabel = new QLabel(m_loginCard);
    m_errorLabel->setObjectName(QStringLiteral("loginErrorLabel"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setFixedHeight(20);
    cardLayout->addWidget(m_errorLabel);

    if (mockMode) {
        auto *mockButtons = new QHBoxLayout();
        auto *staffButton = new QPushButton(QStringLiteral("STAFF Mock"), m_loginCard);
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

    m_loginButton = new QPushButton(QStringLiteral("대시보드 시작 →"), m_loginCard);
    m_loginButton->setObjectName(QStringLiteral("primaryCtaButton"));
    m_loginButton->setDefault(true);
    cardLayout->addWidget(m_loginButton);

#ifdef QT_DEBUG
    auto *demoLabel = new QLabel(QStringLiteral("ⓘ 데모 계정 안내 (디버그 빌드 전용)"), m_loginCard);
#else
    // 릴리스 빌드에서는 계정 정보를 평문으로 노출하지 않고 아이콘 + 툴팁으로 접어서 보여준다.
    auto *demoLabel = new QLabel(QStringLiteral("ⓘ 데모 계정 안내"), m_loginCard);
#endif
    demoLabel->setObjectName(QStringLiteral("demoHint"));
    demoLabel->setToolTip(QStringLiteral("admin / admin123  ·  user / user123"));
    cardLayout->addWidget(demoLabel);

    formAreaLayout->addWidget(m_loginCard);
    formAreaLayout->addStretch();

    rootLayout->addWidget(visual, 12);
    rootLayout->addWidget(formArea, 10);

    // m_loginButton is the dialog's default button (setDefault(true) above), so
    // Return/Enter in either field already triggers it — no separate
    // returnPressed wiring needed (that would double-submit each attempt).
    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::attemptLogin);
    connect(m_passwordVisibilityButton, &QToolButton::clicked, this, &LoginDialog::togglePasswordVisibility);
    repositionPasswordVisibilityButton();
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
    return UserRole::Staff;
}
