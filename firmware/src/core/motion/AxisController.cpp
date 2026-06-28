#include "AxisController.h"
#include <esp_timer.h>

AxisController* AxisController::s_activeInstances[8] = {nullptr};
int AxisController::s_instanceCount = 0;

static esp_timer_handle_t s_stepTimer = nullptr;

static void stepTimerCallback(void* arg) {
    (void)arg;
    for (int i = 0; i < AxisController::s_instanceCount; i++) {
        if (AxisController::s_activeInstances[i]) {
            AxisController::s_activeInstances[i]->doStep();
        }
    }
}

AxisController::AxisController()
    : m_targetPosition(0)
    , m_currentSpeed(0.0f)
    , m_targetSpeed(0.0f)
    , m_acceleration(0.0f)
    , m_lastStepTime(0)
    , m_timerStarted(false) {
    m_state.currentPosition = 0;
    m_state.currentSpeed = 0.0f;
    m_state.isHomed = false;
    m_state.isMoving = false;
    m_state.isEnabled = false;
}

AxisController::~AxisController() {
    detachTimer(this);
    stopTimer();
}

bool AxisController::init(const AxisConfig& config) {
    m_config = config;
    m_state.isEnabled = false;

    pinMode(config.stepPin, OUTPUT);
    pinMode(config.dirPin, OUTPUT);
    pinMode(config.enablePin, OUTPUT);
    if (config.homePin >= 0) {
        pinMode(config.homePin, INPUT_PULLUP);
    }

    digitalWrite(config.stepPin, LOW);
    digitalWrite(config.dirPin, LOW);
    digitalWrite(config.enablePin, HIGH); // disabled (active low)

    m_currentSpeed = 0.0f;
    m_targetSpeed = config.maxSpeed;
    m_acceleration = config.acceleration;

    return attachTimer(this);
}

bool AxisController::moveTo(long targetSteps) {
    if (!m_state.isEnabled) return false;
    m_targetPosition = targetSteps;
    m_state.isMoving = true;
    return true;
}

bool AxisController::moveRelative(long steps) {
    return moveTo(m_state.currentPosition + steps);
}

bool AxisController::stop() {
    m_state.isMoving = false;
    m_targetPosition = m_state.currentPosition;
    m_currentSpeed = 0.0f;
    return true;
}

bool AxisController::home() {
    if (m_config.homePin < 0) {
        m_state.isHomed = true;
        m_state.currentPosition = 0;
        return true;
    }

    if (!m_state.isEnabled) return false;

    setDirection(false);
    m_targetSpeed = m_config.maxSpeed * 0.3f;
    m_state.isMoving = true;

    while (digitalRead(m_config.homePin) == HIGH) {
        delay(1);
    }

    stop();
    m_state.currentPosition = 0;
    m_state.isHomed = true;
    m_targetSpeed = m_config.maxSpeed;
    return true;
}

bool AxisController::setSpeed(float speed) {
    if (speed < 0) return false;
    m_targetSpeed = speed;
    return true;
}

bool AxisController::setAcceleration(float accel) {
    if (accel < 0) return false;
    m_acceleration = accel;
    return true;
}

AxisState AxisController::getState() const {
    return m_state;
}

bool AxisController::isBusy() const {
    return m_state.isMoving;
}

bool AxisController::enable(bool on) {
    m_state.isEnabled = on;
    digitalWrite(m_config.enablePin, on ? LOW : HIGH);
    if (!on) stop();
    return true;
}

bool AxisController::setMicrosteps(int microsteps) {
    (void)microsteps;
    return true;
}

bool AxisController::isHomed() const {
    return m_state.isHomed;
}

long AxisController::getCurrentPosition() const {
    return m_state.currentPosition;
}

void AxisController::startTimer() {
    if (m_timerStarted) return;

    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = &stepTimerCallback;
    timerArgs.name = "step_timer";

    if (esp_timer_create(&timerArgs, &s_stepTimer) == ESP_OK) {
        esp_timer_start_periodic(s_stepTimer, 50); // 50us = 20kHz max step rate
        m_timerStarted = true;
    }
}

void AxisController::stopTimer() {
    if (s_stepTimer && m_timerStarted) {
        esp_timer_stop(s_stepTimer);
        esp_timer_delete(s_stepTimer);
        s_stepTimer = nullptr;
        m_timerStarted = false;
    }
}

void AxisController::doStep() {
    if (!m_state.isMoving || !m_state.isEnabled) return;

    computeSpeed();

    long delta = m_targetPosition - m_state.currentPosition;
    if (delta == 0) {
        m_state.isMoving = false;
        m_currentSpeed = 0.0f;
        return;
    }

    setDirection(delta > 0);
    digitalWrite(m_config.stepPin, HIGH);
    delayMicroseconds(3);
    digitalWrite(m_config.stepPin, LOW);

    if (delta > 0) {
        m_state.currentPosition++;
    } else {
        m_state.currentPosition--;
    }
}

void AxisController::computeSpeed() {
    unsigned long now = micros();
    float dt = (now - m_lastStepTime) / 1000000.0f;
    m_lastStepTime = now;

    long remaining = abs(m_targetPosition - m_state.currentPosition);
    float decelDist = (m_currentSpeed * m_currentSpeed) / (2.0f * m_acceleration);

    if (remaining <= (long)(decelDist * m_config.stepsPerDegree)) {
        m_currentSpeed -= m_acceleration * dt;
        if (m_currentSpeed < 10.0f) m_currentSpeed = 10.0f;
    } else if (m_currentSpeed < m_targetSpeed) {
        m_currentSpeed += m_acceleration * dt;
        if (m_currentSpeed > m_targetSpeed) m_currentSpeed = m_targetSpeed;
    }

    m_state.currentSpeed = m_currentSpeed;
}

void AxisController::setDirection(bool positive) {
    digitalWrite(m_config.dirPin, positive ? HIGH : LOW);
    delayMicroseconds(5);
}

bool AxisController::attachTimer(AxisController* axis) {
    if (!axis || s_instanceCount >= 8) return false;
    s_activeInstances[s_instanceCount++] = axis;
    if (!s_stepTimer) {
        axis->startTimer();
    }
    return true;
}

void AxisController::detachTimer(AxisController* axis) {
    for (int i = 0; i < s_instanceCount; i++) {
        if (s_activeInstances[i] == axis) {
            for (int j = i; j < s_instanceCount - 1; j++) {
                s_activeInstances[j] = s_activeInstances[j + 1];
            }
            s_activeInstances[--s_instanceCount] = nullptr;
            break;
        }
    }
    if (s_instanceCount == 0) {
        axis->stopTimer();
    }
}
