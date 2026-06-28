#ifndef SMARTCAM_AXIS_CONTROLLER_H
#define SMARTCAM_AXIS_CONTROLLER_H

#include <Arduino.h>
#include "../../../sdk/interfaces/IMotionController.h"

class AxisController : public IMotionController {
public:
    AxisController();
    ~AxisController();

    bool init(const AxisConfig& config) override;
    bool moveTo(long targetSteps) override;
    bool moveRelative(long steps) override;
    bool stop() override;
    bool home() override;
    bool setSpeed(float speed) override;
    bool setAcceleration(float accel) override;
    AxisState getState() const override;
    bool isBusy() const override;

    bool enable(bool on);
    bool setMicrosteps(int microsteps);
    bool isHomed() const;
    long getCurrentPosition() const;

    static void onTimer();

private:
    AxisConfig m_config;
    AxisState m_state;
    long m_targetPosition;
    float m_currentSpeed;
    float m_targetSpeed;
    float m_acceleration;
    unsigned long m_lastStepTime;
    int m_timerGroup;
    int m_timerIndex;
    bool m_timerStarted;

    void startTimer();
    void stopTimer();
    void doStep();
    void computeSpeed();
    void setDirection(bool positive);

    static AxisController* s_activeInstances[8];
    static int s_instanceCount;

public:
    static bool attachTimer(AxisController* axis);
    static void detachTimer(AxisController* axis);
};

#endif
