#include "PIDController.h"

PIDController::PIDController()
    : m_integral(0.0f)
    , m_prevError(0.0f)
    , m_outputMin(-100000.0f)
    , m_outputMax(100000.0f)
    , m_intMin(-100000.0f)
    , m_intMax(100000.0f)
    , m_firstRun(true) {}

PIDController::PIDController(const PIDGains& gains)
    : m_gains(gains)
    , m_integral(0.0f)
    , m_prevError(0.0f)
    , m_outputMin(-100000.0f)
    , m_outputMax(100000.0f)
    , m_intMin(-100000.0f)
    , m_intMax(100000.0f)
    , m_firstRun(true) {}

PIDController::~PIDController() {}

void PIDController::setGains(const PIDGains& gains) {
    m_gains = gains;
}

PIDGains PIDController::getGains() const {
    return m_gains;
}

float PIDController::compute(float error, float dt) {
    if (dt <= 0.0f) dt = 0.001f;

    float p = m_gains.kp * error;
    m_integral += error * dt;
    m_integral = clamp(m_integral, m_intMin, m_intMax);

    float d = 0.0f;
    if (!m_firstRun) {
        d = m_gains.kd * (error - m_prevError) / dt;
    } else {
        m_firstRun = false;
    }

    m_prevError = error;
    float output = p + (m_gains.ki * m_integral) + d;
    return clamp(output, m_outputMin, m_outputMax);
}

void PIDController::reset() {
    m_integral = 0.0f;
    m_prevError = 0.0f;
    m_firstRun = true;
}

void PIDController::setOutputLimits(float min, float max) {
    m_outputMin = min;
    m_outputMax = max;
}

void PIDController::setIntegratorLimits(float min, float max) {
    m_intMin = min;
    m_intMax = max;
}

float PIDController::getProportional() const {
    return m_gains.kp * m_prevError;
}

float PIDController::getIntegral() const {
    return m_integral;
}

float PIDController::getDerivative() const {
    return m_gains.kd * m_prevError;
}

float PIDController::clamp(float value, float min, float max) const {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
