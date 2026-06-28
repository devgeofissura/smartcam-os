#ifndef SMARTCAM_PERSON_TRACKER_APP_H
#define SMARTCAM_PERSON_TRACKER_APP_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamApp.h"
#include "../camera/CameraEngine.h"
#include "../vision/VisionEngine.h"
#include "../tracking/TrackingEngine.h"
#include "../motion/MotionEngine.h"
#include "../ai/AIEngine.h"

class PersonTrackerApp : public SmartCamApp {
public:
    bool begin() override;
    void update() override;
    bool stop() override;

    void onEvent(const char* eventType, void* data) override;
    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    bool startTrackingPerson();
    bool startTrackingColor(const char* colorLabel);
    bool stopTracking();

    bool setDetector(const char* name);
    int getTrackingFps() const;

    const char* name() const override { return "PersonTracker"; }

private:
    bool m_enabled;
    bool m_running;
    unsigned long m_lastFrameTime;
    int m_frameCount;
    int m_trackingFps;
    unsigned long m_fpsTimer;

public:
    PersonTrackerApp();
    ~PersonTrackerApp();
};

#endif
