#ifndef SPLASHSCREENWIDGET_H
#define SPLASHSCREENWIDGET_H

#include <QWidget>

class QLabel;
class QMovie;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class SplashScreenContentWidget;
class QTimer;

class SplashScreenWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int kFadeInMs = 400;
    static constexpr int kVisibleDurationMs = 1500;
    static constexpr int kFadeOutMs = 500;

    explicit SplashScreenWidget(QWidget *parent = nullptr);
    ~SplashScreenWidget() override;

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void finished();

public slots:
    void setStatus(const QString &text);
    void setProgress(int percent);
    void notifyReady();
    void closeNow();

private slots:
    void handleMinimumDurationElapsed();
    void startFadeOut();

private:
    void configureWindow();
    void buildUi();
    void centerOnPrimaryScreen();
    void updateMovieScale();
    void startAnimation();
    void startFadeIn();

    SplashScreenContentWidget *m_contentWidget;
    QMovie *m_logoMovie;
    QGraphicsOpacityEffect *m_opacityEffect;
    QPropertyAnimation *m_fadeInAnimation;
    QPropertyAnimation *m_fadeOutAnimation;
    QTimer *m_finishTimer;
    bool m_ready;
    bool m_closing;
    bool m_fadeInStarted;
    bool m_minimumDurationElapsed;
};

#endif // SPLASHSCREENWIDGET_H
