#ifndef SPLASHSCREENCONTENTWIDGET_H
#define SPLASHSCREENCONTENTWIDGET_H

#include <QWidget>

class QLabel;
class QMovie;

class SplashScreenContentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreenContentWidget(QWidget *parent = nullptr);

    void setStatusText(const QString &text);
    void attachMovie(QMovie *movie);
    void showFallbackText(const QString &text);
    void updateMovieScale(qreal zoomFactorX, qreal zoomFactorY);

private:
    QLabel *m_logoLabel;
    QLabel *m_statusLabel;
    QMovie *m_movie;
};

#endif // SPLASHSCREENCONTENTWIDGET_H
