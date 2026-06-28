#include "PersonTrackerApp.h"
#include "../logger/LoggerService.h"
#include <esp_timer.h>

extern LoggerService loggerService;

PersonTrackerApp::PersonTrackerApp()
    : m_enabled(true)
    , m_running(false)
    , m_lastFrameTime(0)
    , m_frameCount(0)
    , m_trackingFps(0)
    , m_fpsTimer(0) {}

PersonTrackerApp::~PersonTrackerApp() {
    stop();
}

bool PersonTrackerApp::begin() {
    if (m_running) return true;

    loggerService.info("App", "PersonTracker v1.0 starting");
    m_running = true;
    m_fpsTimer = millis();
    return true;
}

void PersonTrackerApp::update() {
    if (!m_running || !m_enabled) return;

    unsigned long now = millis();

    if (!cameraEngine.isStreaming()) return;

    uint8_t* frame = nullptr;
    int width = 0, height = 0;
    if (!cameraEngine.getFrame(&frame, &width, &height)) return;

    detectionEngine.update();

    Detection detections[8];
    int count = detectionEngine.getAllDetections(detections, 8);

    if (count > 0) {
        Detection* best = &detections[0];
        for (int i = 1; i < count; i++) {
            if (detections[i].confidence > best->confidence) {
                best = &detections[i];
            }
        }

        trackingEngine.setTarget(*best);
    }

    trackingEngine.update();
    cameraEngine.returnFrame();

    m_frameCount++;
    if (now - m_fpsTimer >= 1000) {
        m_trackingFps = m_frameCount;
        m_frameCount = 0;
        m_fpsTimer = now;
    }
}

bool PersonTrackerApp::stop() {
    trackingEngine.clearTarget();
    m_running = false;
    return true;
}

void PersonTrackerApp::onEvent(const char* eventType, void* data) {
    (void)eventType;
    (void)data;
}

bool PersonTrackerApp::isEnabled() const {
    return m_enabled;
}

void PersonTrackerApp::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!m_enabled) {
        trackingEngine.clearTarget();
    }
}

bool PersonTrackerApp::startTrackingPerson() {
    if (!setDetector("person")) return false;

    Detection initialTarget;
    initialTarget.x = 0.5f;
    initialTarget.y = 0.5f;
    initialTarget.width = 0.1f;
    initialTarget.height = 0.1f;
    initialTarget.confidence = 0.0f;
    initialTarget.classId = 1;
    strcpy(initialTarget.label, "person");
    trackingEngine.setTarget(initialTarget);
    m_enabled = true;

    loggerService.info("App", "Person tracking started");
    return true;
}

bool PersonTrackerApp::startTrackingColor(const char* colorLabel) {
    if (!setDetector(colorLabel)) return false;

    Detection initialTarget;
    initialTarget.x = 0.5f;
    initialTarget.y = 0.5f;
    initialTarget.width = 0.1f;
    initialTarget.height = 0.1f;
    initialTarget.confidence = 0.0f;
    initialTarget.classId = 0;
    strncpy(initialTarget.label, colorLabel, sizeof(initialTarget.label) - 1);
    initialTarget.label[sizeof(initialTarget.label) - 1] = '\0';
    trackingEngine.setTarget(initialTarget);
    m_enabled = true;

    loggerService.info("App", "Color tracking started for %s", colorLabel);
    return true;
}

bool PersonTrackerApp::stopTracking() {
    trackingEngine.clearTarget();
    m_enabled = false;
    loggerService.info("App", "Tracking stopped");
    return true;
}

bool PersonTrackerApp::setDetector(const char* name) {
    IDetector* detector = detectionEngine.getDetector(name);
    if (!detector) {
        loggerService.warning("App", "Unknown detector: %s", name);
        return false;
    }
    return detectionEngine.setActiveDetector(name);
}

int PersonTrackerApp::getTrackingFps() const {
    return m_trackingFps;
}
