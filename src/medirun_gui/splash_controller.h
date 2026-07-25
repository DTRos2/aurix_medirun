#ifndef SPLASH_CONTROLLER_H
#define SPLASH_CONTROLLER_H

#include <QObject>
#include <QPointer>

class QQuickView;

class SplashController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool finish_requested READ finishRequested NOTIFY finishRequestedChanged)

public:
    explicit SplashController(QObject *parent = nullptr);

    void attachView(QQuickView *view);
    void start();
    void markInitializationFinished();
    bool finishRequested() const;

signals:
    void readyToClose();
    void finishRequestedChanged();
    void splashAnimationFinished();

public slots:
    void handleSplashAnimationFinished();

private:
    void updateFinishState();
    void resizeAndCenterView();

    QPointer<QQuickView> view_;
    bool initialization_finished_;
    bool minimum_duration_elapsed_;
    bool finish_requested_;
};

#endif // SPLASH_CONTROLLER_H
