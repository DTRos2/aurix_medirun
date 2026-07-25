#include "splashscreenwidget.h"

#include "splashassetresolver.h"

#include <QDebug>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kSplashWindowWidth = 900;
constexpr int kSplashWindowHeight = 900;
constexpr qreal kMovieZoomFactorX = 9.0;
constexpr qreal kMovieZoomFactorY = 24.0;
constexpr int kStatusAreaHeight = 64;
}

SplashScreenWidget::SplashScreenWidget(QWidget *parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_contentWidget(nullptr)
    , m_logoLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_logoMovie(nullptr)
    , m_opacityEffect(nullptr)
    , m_fadeInAnimation(nullptr)
    , m_fadeOutAnimation(nullptr)
    , m_finishTimer(new QTimer(this))
    , m_ready(false)
    , m_closing(false)
    , m_fadeInStarted(false)
    , m_minimumDurationElapsed(false)
{
    configureWindow();
    buildUi();
    centerOnPrimaryScreen();
    startAnimation();

    m_finishTimer->setSingleShot(true);
    connect(m_finishTimer, &QTimer::timeout, this, &SplashScreenWidget::handleMinimumDurationElapsed);
}

SplashScreenWidget::~SplashScreenWidget() = default;

void SplashScreenWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (!m_fadeInStarted) {
        m_fadeInStarted = true;
        startFadeIn();
        m_finishTimer->start(kVisibleDurationMs);
    }
}

void SplashScreenWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateMovieScale();
}

void SplashScreenWidget::setStatus(const QString &text)
{
    if (m_statusLabel != nullptr) {
        m_statusLabel->setText(text);
    }
}

void SplashScreenWidget::setProgress(int percent)
{
    Q_UNUSED(percent);
}

void SplashScreenWidget::notifyReady()
{
    m_ready = true;
    if (m_minimumDurationElapsed) {
        startFadeOut();
    }
}

void SplashScreenWidget::closeNow()
{
    m_ready = true;
    startFadeOut();
}

void SplashScreenWidget::handleMinimumDurationElapsed()
{
    m_minimumDurationElapsed = true;
    if (m_ready) {
        startFadeOut();
    }
}

void SplashScreenWidget::startFadeOut()
{
    if (m_closing || !m_ready || !m_minimumDurationElapsed) {
        return;
    }

    m_closing = true;
    m_finishTimer->stop();
    m_fadeInAnimation->stop();
    m_fadeOutAnimation->setStartValue(m_opacityEffect->opacity());
    m_fadeOutAnimation->start();
}

void SplashScreenWidget::configureWindow()
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("splashRoot"));
    setStyleSheet(QStringLiteral("QWidget#splashRoot { background-color: #000000; }"));
    setFixedSize(kSplashWindowWidth, kSplashWindowHeight);
}

void SplashScreenWidget::buildUi()
{
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_contentWidget = new QWidget(this);
    m_contentWidget->setObjectName(QStringLiteral("splashContent"));
    m_contentWidget->setStyleSheet(QStringLiteral("QWidget#splashContent { background-color: #000000; }"));
    rootLayout->addWidget(m_contentWidget);

    auto *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_logoLabel = new QLabel(m_contentWidget);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setStyleSheet(QStringLiteral("background-color: #000000;"));
    m_logoLabel->setScaledContents(false);
    contentLayout->addWidget(m_logoLabel, 1);

    m_statusLabel = new QLabel(QStringLiteral("운영 대시보드를 불러오는 중입니다..."), m_contentWidget);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setFixedHeight(kStatusAreaHeight);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "background-color: #000000;"
        "color: #F2C35A;"
        "font-size: 18px;"
        "font-weight: 600;"));
    contentLayout->addWidget(m_statusLabel, 0);

    m_opacityEffect = new QGraphicsOpacityEffect(m_contentWidget);
    m_opacityEffect->setOpacity(0.0);
    m_contentWidget->setGraphicsEffect(m_opacityEffect);

    m_fadeInAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeInAnimation->setDuration(kFadeInMs);
    m_fadeInAnimation->setStartValue(0.0);
    m_fadeInAnimation->setEndValue(1.0);

    m_fadeOutAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeOutAnimation->setDuration(kFadeOutMs);
    m_fadeOutAnimation->setStartValue(1.0);
    m_fadeOutAnimation->setEndValue(0.0);

    connect(m_fadeOutAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_logoMovie != nullptr) {
            m_logoMovie->stop();
        }
        hide();
        emit finished();
    });
}

void SplashScreenWidget::centerOnPrimaryScreen()
{
    if (QScreen *screen = QApplication::primaryScreen()) {
        move(screen->availableGeometry().center() - rect().center());
    }
}

void SplashScreenWidget::updateMovieScale()
{
    if (m_logoMovie == nullptr) {
        return;
    }

    const QSize contentSize = m_logoLabel->size();
    if (!contentSize.isValid()) {
        return;
    }

    const QSize zoomedSize(
        static_cast<int>(contentSize.width() * kMovieZoomFactorX),
        static_cast<int>(contentSize.height() * kMovieZoomFactorY));
    m_logoMovie->setScaledSize(zoomedSize);
}

void SplashScreenWidget::startAnimation()
{
    const QString animationPath = SplashAssetResolver::resolveSplashAnimationPath();
    if (animationPath.isEmpty()) {
        qDebug() << "Failed to resolve splash GIF resource path:" << QStringLiteral(":/assets/aurix_splash.gif");
        m_logoLabel->setText(QStringLiteral("AURIX"));
        m_logoLabel->setStyleSheet(QStringLiteral(
            "background-color: #000000;"
            "color: #E8C979;"
            "font-size: 52px;"
            "font-weight: 700;"));
        return;
    }

    m_logoMovie = new QMovie(animationPath, QByteArray(), this);
    if (!m_logoMovie->isValid()) {
        qDebug() << "Failed to load splash GIF:" << animationPath;
        m_logoLabel->setText(QStringLiteral("AURIX"));
        m_logoLabel->setStyleSheet(QStringLiteral(
            "background-color: #000000;"
            "color: #E8C979;"
            "font-size: 52px;"
            "font-weight: 700;"));
        m_logoMovie->deleteLater();
        m_logoMovie = nullptr;
        return;
    }

    qDebug() << "Loaded splash GIF from:" << animationPath;

    m_logoMovie->setCacheMode(QMovie::CacheAll);
    m_logoLabel->setMovie(m_logoMovie);
    updateMovieScale();
    m_logoMovie->start();
}

void SplashScreenWidget::startFadeIn()
{
    m_fadeInAnimation->start();
}
