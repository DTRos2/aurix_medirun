#include "splash_controller.h"

#include <QGuiApplication>
#include <QQuickView>
#include <QScreen>
#include <QTimer>
#include <QtGlobal>

namespace {
constexpr int kDefaultSplashSize = 900;
constexpr int kMinimumDurationMs = 3000;
constexpr int kFallbackInitializationMs = 4000;
constexpr qreal kScreenUsageRatio = 0.86;
}

SplashController::SplashController(QObject *parent)
    : QObject(parent)
    , initialization_finished_(false)
    , minimum_duration_elapsed_(false)
    , finish_requested_(false)
{
}

void SplashController::attachView(QQuickView *view)
{
    view_ = view;
}

void SplashController::start()
{
    if (view_ != nullptr) {
        resizeAndCenterView();
        view_->show();
    }

    QTimer::singleShot(kMinimumDurationMs, this, [this]() {
        minimum_duration_elapsed_ = true;
        updateFinishState();
    });

    QTimer::singleShot(kFallbackInitializationMs, this, [this]() {
        markInitializationFinished();
    });
}

void SplashController::markInitializationFinished()
{
    if (initialization_finished_) {
        return;
    }

    initialization_finished_ = true;
    updateFinishState();
}

bool SplashController::finishRequested() const
{
    return finish_requested_;
}

void SplashController::handleSplashAnimationFinished()
{
    emit splashAnimationFinished();
}

void SplashController::updateFinishState()
{
    if (finish_requested_ || !initialization_finished_ || !minimum_duration_elapsed_) {
        return;
    }

    finish_requested_ = true;
    emit finishRequestedChanged();
    emit readyToClose();
}

void SplashController::resizeAndCenterView()
{
    if (view_ == nullptr) {
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        view_->resize(kDefaultSplashSize, kDefaultSplashSize);
        return;
    }

    const QRect available_geometry = screen->availableGeometry();
    const int max_square_size = qMin(
        kDefaultSplashSize,
        static_cast<int>(qMin(available_geometry.width(), available_geometry.height()) * kScreenUsageRatio));
    const int splash_size = qMax(480, max_square_size);

    view_->resize(splash_size, splash_size);
    const QPoint top_left = available_geometry.center() - QPoint(splash_size / 2, splash_size / 2);
    view_->setPosition(top_left);
}
