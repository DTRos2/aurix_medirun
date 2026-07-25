#include "splashscreenwidget.h"

#include "splashassetresolver.h"
#include "splashscreencontentwidget.h"

#include <QDebug>
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QMovie>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>

namespace {
constexpr int kSplashWindowWidth = 900;
constexpr int kSplashWindowHeight = 900;
constexpr qreal kMovieZoomFactorX = 9.0;
constexpr qreal kMovieZoomFactorY = 24.0;
}

SplashScreenWidget::SplashScreenWidget(QWidget *parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_contentWidget(nullptr)
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
    if (m_contentWidget != nullptr) {
        m_contentWidget->setStatusText(text);
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

    m_contentWidget = new SplashScreenContentWidget(this);
    rootLayout->addWidget(m_contentWidget);

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
    if (m_logoMovie == nullptr || m_contentWidget == nullptr) {
        return;
    }

    m_contentWidget->updateMovieScale(kMovieZoomFactorX, kMovieZoomFactorY);
}

void SplashScreenWidget::startAnimation()
{
    const QString animationPath = SplashAssetResolver::resolveSplashAnimationPath();
    if (animationPath.isEmpty()) {
        qDebug() << "Failed to resolve splash GIF resource path:" << QStringLiteral(":/assets/aurix_splash.gif");
        m_contentWidget->showFallbackText(QStringLiteral("AURIX"));
        return;
    }

    m_logoMovie = new QMovie(animationPath, QByteArray(), this);
    if (!m_logoMovie->isValid()) {
        qDebug() << "Failed to load splash GIF:" << animationPath;
        m_contentWidget->showFallbackText(QStringLiteral("AURIX"));
        m_logoMovie->deleteLater();
        m_logoMovie = nullptr;
        return;
    }

    qDebug() << "Loaded splash GIF from:" << animationPath;

    m_logoMovie->setCacheMode(QMovie::CacheAll);
    m_contentWidget->attachMovie(m_logoMovie);
    updateMovieScale();
    m_logoMovie->start();
}

void SplashScreenWidget::startFadeIn()
{
    m_fadeInAnimation->start();
}
