#include "splashscreencontentwidget.h"

#include <QLabel>
#include <QMovie>
#include <QVBoxLayout>

namespace {
constexpr int kStatusAreaHeight = 64;
}

SplashScreenContentWidget::SplashScreenContentWidget(QWidget *parent)
    : QWidget(parent)
    , m_logoLabel(new QLabel(this))
    , m_statusLabel(new QLabel(QStringLiteral("운영 대시보드를 불러오는 중입니다..."), this))
    , m_movie(nullptr)
{
    setObjectName(QStringLiteral("splashContent"));
    setStyleSheet(QStringLiteral("QWidget#splashContent { background-color: #000000; }"));

    auto *contentLayout = new QVBoxLayout(this);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setStyleSheet(QStringLiteral("background-color: #000000;"));
    m_logoLabel->setScaledContents(false);
    contentLayout->addWidget(m_logoLabel, 1);

    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setFixedHeight(kStatusAreaHeight);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "background-color: #000000;"
        "color: #F2C35A;"
        "font-size: 18px;"
        "font-weight: 600;"));
    contentLayout->addWidget(m_statusLabel, 0);
}

void SplashScreenContentWidget::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

void SplashScreenContentWidget::attachMovie(QMovie *movie)
{
    m_movie = movie;
    m_logoLabel->setMovie(movie);
}

void SplashScreenContentWidget::showFallbackText(const QString &text)
{
    m_movie = nullptr;
    m_logoLabel->setMovie(nullptr);
    m_logoLabel->setText(text);
    m_logoLabel->setStyleSheet(QStringLiteral(
        "background-color: #000000;"
        "color: #E8C979;"
        "font-size: 52px;"
        "font-weight: 700;"));
}

void SplashScreenContentWidget::updateMovieScale(qreal zoomFactorX, qreal zoomFactorY)
{
    if (m_movie == nullptr) {
        return;
    }

    const QSize contentSize = m_logoLabel->size();
    if (!contentSize.isValid()) {
        return;
    }

    const QSize zoomedSize(
        static_cast<int>(contentSize.width() * zoomFactorX),
        static_cast<int>(contentSize.height() * zoomFactorY));
    m_movie->setScaledSize(zoomedSize);
}
