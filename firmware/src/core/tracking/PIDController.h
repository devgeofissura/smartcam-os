#ifndef SMARTCAM_PID_CONTROLLER_H
#define SMARTCAM_PID_CONTROLLER_H

#include <Arduino.h>

struct PIDGains {
    float kp;
    float ki;
    float kd;
    float kf;

    PIDGains() : kp(1.0f), ki(0.0f), kd(0.1f), kf(0.0f) {}
    PIDGains(float p, float i, float d, float f = 0.0f)
        : kp(p), ki(i), kd(d), kf(f) {}
};

class PIDController {
public:
    PIDController();
    explicit PIDController(const PIDGains& gains);
    ~PIDController();

    void setGains(const PIDGains& gains);
    PIDGains getGains() const;

    float compute(float error, float dt);
    void reset();
    void setOutputLimits(float min, float max);
    void setIntegratorLimits(float min, float max);

    float getProportional() const;
    float getIntegral() const;
    float getDerivative() const;

private:
    PIDGains m_gains;
    float m_integral;
    float m_prevError;
    float m_outputMin;
    float m_outputMax;
    float m_intMin;
    float m_intMax;
    bool m_firstRun;

    float clamp(float value, float min, float max) const;
};

#endif
