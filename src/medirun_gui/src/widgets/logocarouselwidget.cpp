#include "logocarouselwidget.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QDebug>

namespace {
constexpr int kButtonSize = 48;
constexpr int kButtonMargin = 20;
constexpr int kFadeDurationMs = 200;
constexpr int kAutoSlideIntervalMs = 5000;
constexpr int kImageMargin = 72;
}

LogoCarouselWidget::LogoCarouselWidget(QWidget *parent)
    : QWidget(parent)
    , m_imageLabel(new QLabel(this))
    , m_errorLabel(new QLabel(this))
    , m_previousButton(new QPushButton(QString::fromUtf8("‹"), this))
    , m_nextButton(new QPushButton(QString::fromUtf8("›"), this))
    , m_opacityEffect(new QGraphicsOpacityEffect(this))
    , m_fadeOutAnimation(new QPropertyAnimation(m_opacityEffect, "opacity", this))
    , m_fadeInAnimation(new QPropertyAnimation(m_opacityEffect, "opacity", this))
    , m_autoSlideTimer(new QTimer(this))
    , m_currentIndex(-1)
    , m_targetIndex(-1)
    , m_isAnimating(false)
    , m_isHovered(false)
{
    setObjectName(QStringLiteral("logoCarouselWidget"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setStyleSheet(QStringLiteral(
        "#logoCarouselWidget { background-color: #050505; }"
        "QPushButton {"
        "background-color: rgba(20, 20, 20, 160);"
        "color: white;"
        "border: 1px solid rgba(255, 255, 255, 40);"
        "border-radius: 8px;"
        "font-size: 28px;"
        "font-weight: 600;"
        "padding-bottom: 2px;"
        "}"
        "QPushButton:hover {"
        "background-color: rgba(70, 70, 70, 200);"
        "border: 1px solid rgba(255, 190, 40, 180);"
        "}"
        "QPushButton:pressed {"
        "background-color: rgba(10, 10, 10, 220);"
        "}"
    ));

    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_imageLabel->setGraphicsEffect(m_opacityEffect);

    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #f0d38a; font-size: 24px;"));
    m_errorLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_errorLabel->hide();

    m_previousButton->setCursor(Qt::PointingHandCursor);
    m_nextButton->setCursor(Qt::PointingHandCursor);
    m_previousButton->setFocusPolicy(Qt::NoFocus);
    m_nextButton->setFocusPolicy(Qt::NoFocus);

    m_fadeOutAnimation->setDuration(kFadeDurationMs);
    m_fadeOutAnimation->setStartValue(1.0);
    m_fadeOutAnimation->setEndValue(0.0);

    m_fadeInAnimation->setDuration(kFadeDurationMs);
    m_fadeInAnimation->setStartValue(0.0);
    m_fadeInAnimation->setEndValue(1.0);

    m_autoSlideTimer->setInterval(kAutoSlideIntervalMs);

    connect(m_previousButton, &QPushButton::clicked, this, &LogoCarouselWidget::showPreviousImage);
    connect(m_nextButton, &QPushButton::clicked, this, &LogoCarouselWidget::showNextImage);
    connect(m_fadeOutAnimation, &QPropertyAnimation::finished, this, &LogoCarouselWidget::handleFadeOutFinished);
    connect(m_fadeInAnimation, &QPropertyAnimation::finished, this, &LogoCarouselWidget::handleFadeInFinished);
    connect(m_autoSlideTimer, &QTimer::timeout, this, &LogoCarouselWidget::advanceAutomatically);

    m_opacityEffect->setOpacity(1.0);
    updateControls();
}

void LogoCarouselWidget::setImagePaths(const QStringList &imagePaths)
{
    m_imagePaths = imagePaths;
    m_currentIndex = m_imagePaths.isEmpty() ? -1 : 0;
    m_targetIndex = m_currentIndex;
    m_isAnimating = false;

    m_fadeOutAnimation->stop();
    m_fadeInAnimation->stop();
    m_opacityEffect->setOpacity(1.0);

    updateCurrentImage(true);
    updateControls();
    updateLayoutGeometry();
}

void LogoCarouselWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayoutGeometry();
}

void LogoCarouselWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        showPreviousImage();
        return;
    case Qt::Key_Right:
        showNextImage();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void LogoCarouselWidget::enterEvent(QEnterEvent *event)
{
    m_isHovered = true;
    m_autoSlideTimer->stop();
    QWidget::enterEvent(event);
}

void LogoCarouselWidget::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    restartAutoSlideTimer();
    QWidget::leaveEvent(event);
}

void LogoCarouselWidget::showPreviousImage()
{
    if (m_imagePaths.size() <= 1) {
        return;
    }

    startTransitionToIndex(m_currentIndex - 1);
}

void LogoCarouselWidget::showNextImage()
{
    if (m_imagePaths.size() <= 1) {
        return;
    }

    startTransitionToIndex(m_currentIndex + 1);
}

void LogoCarouselWidget::handleFadeOutFinished()
{
    if (m_targetIndex < 0 || m_targetIndex >= m_imagePaths.size()) {
        m_isAnimating = false;
        updateControls();
        return;
    }

    m_currentIndex = m_targetIndex;
    updateCurrentImage(true);
    updateLayoutGeometry();
    m_fadeInAnimation->start();
}

void LogoCarouselWidget::handleFadeInFinished()
{
    m_isAnimating = false;
    updateControls();
    restartAutoSlideTimer();
}

void LogoCarouselWidget::advanceAutomatically()
{
    if (m_isHovered || m_isAnimating || m_imagePaths.size() <= 1) {
        return;
    }

    startTransitionToIndex(m_currentIndex + 1);
}

void LogoCarouselWidget::updateLayoutGeometry()
{
    m_errorLabel->setGeometry(rect());

    const QSize buttonSize(kButtonSize, kButtonSize);
    const int buttonY = (height() - buttonSize.height()) / 2;
    m_previousButton->setGeometry(kButtonMargin, buttonY, buttonSize.width(), buttonSize.height());
    m_nextButton->setGeometry(width() - kButtonMargin - buttonSize.width(), buttonY, buttonSize.width(), buttonSize.height());

    updateScaledPixmap();
    m_previousButton->raise();
    m_nextButton->raise();
}

void LogoCarouselWidget::updateCurrentImage(bool logOnFailure)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_imagePaths.size()) {
        showErrorMessage(QString::fromUtf8("로고 이미지를 불러올 수 없습니다."));
        return;
    }

    const QString &imagePath = m_imagePaths.at(m_currentIndex);
    const QPixmap pixmap = loadPixmapForPath(imagePath);
    if (pixmap.isNull()) {
        if (logOnFailure) {
            qWarning() << "Failed to load logo image resource:" << imagePath;
        }
        showErrorMessage(QString::fromUtf8("로고 이미지를 불러올 수 없습니다."));
        return;
    }

    m_originalPixmap = pixmap;
    m_errorLabel->hide();
    m_imageLabel->show();
    updateScaledPixmap();
}

QPixmap LogoCarouselWidget::loadPixmapForPath(const QString &imagePath) const
{
    const QString normalizedPath = imagePath.trimmed();
    if (normalizedPath.isEmpty()) {
        return QPixmap();
    }

    const QPixmap resourcePixmap(normalizedPath);
    if (!resourcePixmap.isNull()) {
        qWarning() << "Loaded logo from resource path:" << normalizedPath;
        return resourcePixmap;
    }

    QString relativePath = normalizedPath;
    if (relativePath.startsWith(QStringLiteral(":/"))) {
        relativePath.remove(0, 2);
    }

    const QString applicationDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();
    const QStringList candidatePaths = {
        QDir(currentDir).filePath(relativePath),
        QDir(applicationDir).filePath(relativePath),
        QDir(applicationDir).filePath(QStringLiteral("../") + relativePath),
        QDir(applicationDir).filePath(QStringLiteral("../../") + relativePath),
        QDir(applicationDir).filePath(QStringLiteral("../../../") + relativePath)
    };

    for (const QString &candidatePath : candidatePaths) {
        if (!QFileInfo::exists(candidatePath)) {
            continue;
        }

        const QPixmap filePixmap(candidatePath);
        if (!filePixmap.isNull()) {
            qWarning() << "Loaded logo from file path fallback:" << candidatePath;
            return filePixmap;
        }
    }

    qWarning() << "Logo lookup failed. Requested path:" << normalizedPath;
    qWarning() << "Application dir:" << applicationDir;
    qWarning() << "Current dir:" << currentDir;
    qWarning() << "Tried relative path:" << relativePath;
    return QPixmap();
}

void LogoCarouselWidget::updateScaledPixmap()
{
    if (m_originalPixmap.isNull()) {
        m_imageLabel->clear();
        m_imageLabel->setGeometry(rect());
        return;
    }

    const int availableWidth = qMax(1, width() - (kImageMargin * 2));
    const int availableHeight = qMax(1, height() - (kImageMargin * 2));
    const QSize targetSize(availableWidth, availableHeight);
    const QPixmap scaledPixmap = m_originalPixmap.scaled(
        targetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);

    const QSize pixmapSize = scaledPixmap.size();
    const QRect imageRect(
        (width() - pixmapSize.width()) / 2,
        (height() - pixmapSize.height()) / 2,
        pixmapSize.width(),
        pixmapSize.height());

    m_imageLabel->setGeometry(imageRect);
    m_imageLabel->setPixmap(scaledPixmap);
}

void LogoCarouselWidget::updateControls()
{
    const bool multipleImages = m_imagePaths.size() > 1;
    m_previousButton->setVisible(multipleImages);
    m_nextButton->setVisible(multipleImages);
    m_previousButton->setEnabled(multipleImages && !m_isAnimating);
    m_nextButton->setEnabled(multipleImages && !m_isAnimating);

    if (!multipleImages) {
        m_autoSlideTimer->stop();
        return;
    }

    restartAutoSlideTimer();
}

void LogoCarouselWidget::startTransitionToIndex(int newIndex)
{
    if (m_isAnimating || m_imagePaths.size() <= 1) {
        return;
    }

    const int normalized = normalizedIndex(newIndex);
    if (normalized == m_currentIndex) {
        restartAutoSlideTimer();
        return;
    }

    m_targetIndex = normalized;
    m_isAnimating = true;
    updateControls();
    m_autoSlideTimer->stop();
    m_fadeInAnimation->stop();
    m_fadeOutAnimation->start();
}

void LogoCarouselWidget::restartAutoSlideTimer()
{
    if (m_isHovered || m_isAnimating || m_imagePaths.size() <= 1) {
        m_autoSlideTimer->stop();
        return;
    }

    m_autoSlideTimer->start();
}

void LogoCarouselWidget::showErrorMessage(const QString &message)
{
    m_originalPixmap = QPixmap();
    m_imageLabel->clear();
    m_imageLabel->hide();
    m_errorLabel->setText(message);
    m_errorLabel->show();
}

int LogoCarouselWidget::normalizedIndex(int index) const
{
    if (m_imagePaths.isEmpty()) {
        return -1;
    }

    const int count = m_imagePaths.size();
    const int normalized = index % count;
    return normalized < 0 ? normalized + count : normalized;
}
