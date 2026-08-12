#ifndef TELEOPCONTROLLER_H
#define TELEOPCONTROLLER_H

#include <QObject>

class QTimer;

// ROS와 무관한 순수 Qt teleop 상태 머신 — 방향 버튼/키보드 입력을 모아
// linear/angular 속도 커맨드로 변환하고, 입력이 지속되는 동안 일정 주기로
// 재발행 신호를 낸다. 실제 /cmd_vel 발행은 velocityCommandChanged 신호를
// 받는 쪽(NavigationService 등)이 담당한다.
class TeleopController : public QObject
{
    Q_OBJECT

public:
    enum class Direction { Forward, Backward, Left, Right };
    enum class SpeedPreset { Fine, Normal, Fast };

    explicit TeleopController(QObject *parent = nullptr);

    void pressDirection(Direction direction);
    void releaseDirection(Direction direction);
    void stop();

    void setSpeedPreset(SpeedPreset preset);
    SpeedPreset speedPreset() const { return m_speedPreset; }
    static double linearSpeedForPreset(SpeedPreset preset);
    static double angularSpeedForPreset(SpeedPreset preset);

    // Qt::Key_* 값을 받아 처리했으면 true. W/A/S/D + 방향키 + Space.
    bool consumeKey(int key);
    bool releaseKey(int key);

signals:
    void velocityCommandChanged(double linearX, double angularZ);
    void activeChanged(bool active);
    void speedPresetChanged(SpeedPreset preset);

private slots:
    void handleRepeatTick();

private:
    void applyDirectionState();
    void publishCommand(double linearX, double angularZ);

    QTimer *m_repeatTimer;
    SpeedPreset m_speedPreset {SpeedPreset::Normal};
    double m_linearX {0.0};
    double m_angularZ {0.0};
    bool m_forwardPressed {false};
    bool m_backwardPressed {false};
    bool m_leftPressed {false};
    bool m_rightPressed {false};
    bool m_active {false};
};

#endif // TELEOPCONTROLLER_H
