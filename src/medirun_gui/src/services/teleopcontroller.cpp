#include "teleopcontroller.h"

#include <QTimer>
#include <QtGlobal>

namespace {
// SLAM 매핑 페이지에서 실기 검증된 고정값(0.06 m/s / 0.20 rad/s)을 "보통"
// 기준으로 삼아 3단 스케일링한 잠정치 — 실주행 테스트 후 조정 필요.
constexpr double kFineLinearMps = 0.04;
constexpr double kFineAngularRadPerSec = 0.15;
constexpr double kNormalLinearMps = 0.08;
constexpr double kNormalAngularRadPerSec = 0.30;
constexpr double kFastLinearMps = 0.15;
constexpr double kFastAngularRadPerSec = 0.50;
constexpr int kRepeatIntervalMs = 100;
} // namespace

TeleopController::TeleopController(QObject *parent)
    : QObject(parent)
    , m_repeatTimer(new QTimer(this))
{
    m_repeatTimer->setInterval(kRepeatIntervalMs);
    connect(m_repeatTimer, &QTimer::timeout, this, &TeleopController::handleRepeatTick);
}

double TeleopController::linearSpeedForPreset(SpeedPreset preset)
{
    switch (preset) {
    case SpeedPreset::Fine:
        return kFineLinearMps;
    case SpeedPreset::Fast:
        return kFastLinearMps;
    case SpeedPreset::Normal:
    default:
        return kNormalLinearMps;
    }
}

double TeleopController::angularSpeedForPreset(SpeedPreset preset)
{
    switch (preset) {
    case SpeedPreset::Fine:
        return kFineAngularRadPerSec;
    case SpeedPreset::Fast:
        return kFastAngularRadPerSec;
    case SpeedPreset::Normal:
    default:
        return kNormalAngularRadPerSec;
    }
}

void TeleopController::setSpeedPreset(SpeedPreset preset)
{
    if (m_speedPreset == preset) {
        return;
    }
    m_speedPreset = preset;
    emit speedPresetChanged(m_speedPreset);
    if (m_active) {
        applyDirectionState();
    }
}

void TeleopController::pressDirection(Direction direction)
{
    switch (direction) {
    case Direction::Forward:
        m_forwardPressed = true;
        m_backwardPressed = false;
        break;
    case Direction::Backward:
        m_backwardPressed = true;
        m_forwardPressed = false;
        break;
    case Direction::Left:
        m_leftPressed = true;
        m_rightPressed = false;
        break;
    case Direction::Right:
        m_rightPressed = true;
        m_leftPressed = false;
        break;
    }
    applyDirectionState();
}

void TeleopController::releaseDirection(Direction direction)
{
    switch (direction) {
    case Direction::Forward:
    case Direction::Backward:
        // 직진/후진은 한 번 입력(클릭/키 프레스)으로 정지 명령이 올 때까지
        // 계속 이동 — 버튼을 떼거나 키를 놓아도 멈추지 않는다. 정지는
        // stop()(Space 또는 정지 버튼)으로만 한다.
        return;
    case Direction::Left:
        m_leftPressed = false;
        break;
    case Direction::Right:
        m_rightPressed = false;
        break;
    }
    applyDirectionState();
}

void TeleopController::stop()
{
    m_repeatTimer->stop();
    m_forwardPressed = false;
    m_backwardPressed = false;
    m_leftPressed = false;
    m_rightPressed = false;
    m_linearX = 0.0;
    m_angularZ = 0.0;
    publishCommand(0.0, 0.0);
    if (m_active) {
        m_active = false;
        emit activeChanged(false);
    }
}

void TeleopController::applyDirectionState()
{
    const double linearSpeed = linearSpeedForPreset(m_speedPreset);
    const double angularSpeed = angularSpeedForPreset(m_speedPreset);

    double linearX = 0.0;
    if (m_forwardPressed != m_backwardPressed) {
        linearX = m_forwardPressed ? linearSpeed : -linearSpeed;
    }

    double angularZ = 0.0;
    if (m_leftPressed != m_rightPressed) {
        angularZ = m_leftPressed ? angularSpeed : -angularSpeed;
    }

    if (qFuzzyIsNull(linearX) && qFuzzyIsNull(angularZ)) {
        stop();
        return;
    }

    m_linearX = linearX;
    m_angularZ = angularZ;
    publishCommand(linearX, angularZ);
    m_repeatTimer->start();

    if (!m_active) {
        m_active = true;
        emit activeChanged(true);
    }
}

void TeleopController::handleRepeatTick()
{
    publishCommand(m_linearX, m_angularZ);
}

void TeleopController::publishCommand(double linearX, double angularZ)
{
    emit velocityCommandChanged(linearX, angularZ);
}

bool TeleopController::consumeKey(int key)
{
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_W:
        pressDirection(Direction::Forward);
        return true;
    case Qt::Key_Down:
    case Qt::Key_S:
        pressDirection(Direction::Backward);
        return true;
    case Qt::Key_Left:
    case Qt::Key_A:
        pressDirection(Direction::Left);
        return true;
    case Qt::Key_Right:
    case Qt::Key_D:
        pressDirection(Direction::Right);
        return true;
    case Qt::Key_Space:
        stop();
        return true;
    default:
        return false;
    }
}

bool TeleopController::releaseKey(int key)
{
    switch (key) {
    case Qt::Key_Up:
    case Qt::Key_W:
        releaseDirection(Direction::Forward);
        return true;
    case Qt::Key_Down:
    case Qt::Key_S:
        releaseDirection(Direction::Backward);
        return true;
    case Qt::Key_Left:
    case Qt::Key_A:
        releaseDirection(Direction::Left);
        return true;
    case Qt::Key_Right:
    case Qt::Key_D:
        releaseDirection(Direction::Right);
        return true;
    default:
        return false;
    }
}
