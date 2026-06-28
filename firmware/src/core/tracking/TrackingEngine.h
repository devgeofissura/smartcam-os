#ifndef SMARTCAM_TRACKING_ENGINE_H
#define SMARTCAM_TRACKING_ENGINE_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamModule.h"
#include "../../sdk/interfaces/ITrackingStrategy.h"
#include "../../sdk/interfaces/IDetector.h"
#include "PIDController.h"

struct TrackingConfig {
    float deadZone = 0.05f;
    float maxSpeed = 100.0f;
    float kp = 1.5f;
    float ki = 0.2f;
    float kd = 0.5f;
    int targetLostTimeoutMs = 2000;
    int searchHomeDelayMs = 5000;
};

class TrackingEngine : public SmartCamModule {
public:
    bool begin() override;
    void update() override;
    bool stop() override;

    bool setTarget(const Detection& detection);
    bool clearTarget();
    bool isTargetLocked() const;
    float getCorrectionAngle() const;
    bool setConfig(const TrackingConfig& config);
    TrackingConfig getConfig() const;
    bool setStrategy(ITrackingStrategy* strategy);

    int getTargetX() const;
    int getTargetY() const;
    int getTargetWidth() const;
    int getTargetHeight() const;
    float getTargetConfidence() const;

    const char* name() const override { return "TrackingEngine"; }

private:
    enum class TrackState {
        Idle,
        Tracking,
        TargetLost,
        Searching
    };

    TrackingConfig m_config;
    PIDController m_pidX;
    PIDController m_pidY;
    TrackState m_state;
    Detection m_target;
    bool m_hasTarget;
    float m_correctionX;
    float m_correctionY;
    unsigned long m_lastSeenTime;
    unsigned long m_searchStartTime;
    bool m_running;

    void updateTracking(float dt);
    void updateSearching(float dt);

public:
    TrackingEngine();
    ~TrackingEngine();
};

#endif
