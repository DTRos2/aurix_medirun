#ifndef LOGOCAROUSELWIDGET_H
#define LOGOCAROUSELWIDGET_H

#include <QPixmap>
#include <QEvent>
#include <QWidget>
#include <QStringList>

class QLabel;
class QPushButton;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;
class QResizeEvent;
class QKeyEvent;
class QEnterEvent;

class LogoCarouselWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogoCarouselWidget(QWidget *parent = nullptr);

    void setImagePaths(const QStringList &imagePaths);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void showPreviousImage();
    void showNextImage();
    void handleFadeOutFinished();
    void handleFadeInFinished();
    void advanceAutomatically();

private:
    // 현재 위젯 크기에 맞춰 로고와 버튼 위치를 다시 계산한다.
    void updateLayoutGeometry();
    // 현재 인덱스의 이미지를 로드하고 표시 상태를 갱신한다.
    void updateCurrentImage(bool logOnFailure);
    // 리소스 경로 실패 시 실행 위치 기준 파일 경로까지 포함해 로고를 찾는다.
    QPixmap loadPixmapForPath(const QString &imagePath) const;
    // 원본 이미지를 기준으로 비율을 유지한 채 다시 스케일링한다.
    void updateScaledPixmap();
    // 버튼 표시 상태와 자동 슬라이드 동작을 함께 정리한다.
    void updateControls();
    // 다음에 표시할 인덱스를 계산하고 전환 애니메이션을 시작한다.
    void startTransitionToIndex(int newIndex);
    // 자동 슬라이드 타이머를 현재 상태에 맞게 다시 시작한다.
    void restartAutoSlideTimer();
    // 에러 문구를 표시하고 이미지 출력을 비운다.
    void showErrorMessage(const QString &message);
    // 유효한 이미지 인덱스로 정규화한다.
    int normalizedIndex(int index) const;

    QLabel *m_imageLabel;
    QLabel *m_errorLabel;
    QPushButton *m_previousButton;
    QPushButton *m_nextButton;
    QGraphicsOpacityEffect *m_opacityEffect;
    QPropertyAnimation *m_fadeOutAnimation;
    QPropertyAnimation *m_fadeInAnimation;
    QTimer *m_autoSlideTimer;
    QStringList m_imagePaths;
    QPixmap m_originalPixmap;
    int m_currentIndex;
    int m_targetIndex;
    bool m_isAnimating;
    bool m_isHovered;
};

#endif // LOGOCAROUSELWIDGET_H
