#ifndef SMARTCAM_MOTION_ENGINE_H
#define SMARTCAM_MOTION_ENGINE_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamModule.h"
#include "../../sdk/interfaces/IMotionController.h"

struct MotionProfile {
    float maxSpeed;
    float acceleration;
    float deceleration;
    long targetPosition;
};

class MotionEngine : public SmartCamModule {
public:
    bool begin() override;
    void update() override;
    bool stop() override;

    bool addAxis(const AxisConfig& config);
    bool removeAxis(int index);
    int getAxisCount() const;
    IMotionController* getAxis(int index);

    bool moveTo(int axisIndex, long position);
    bool moveRelative(int axisIndex, long steps);
    bool stopAxis(int axisIndex);
    bool homeAxis(int axisIndex);
    bool enableAxis(int axisIndex, bool enable);
    bool setProfile(int axisIndex, const MotionProfile& profile);
    AxisState getAxisState(int axisIndex) const;

    const char* name() const override { return "MotionEngine"; }

private:
    static const int MAX_AXES = 4;
    class AxisController* m_axes[MAX_AXES];
    int m_axisCount;

public:
    MotionEngine();
    ~MotionEngine();
};

#endif
