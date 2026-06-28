#include "TrackingEngine.h"
#include "../motion/MotionEngine.h"
#include "../ai/AIEngine.h"
#include <math.h>

static TrackingEngine* s_instance = nullptr;

extern MotionEngine motionEngine;
extern DetectionEngine detectionEngine;

TrackingEngine::TrackingEngine()
    : m_state(TrackState::Idle)
    , m_hasTarget(false)
    , m_correctionX(0.0f)
    , m_correctionY(0.0f)
    , m_lastSeenTime(0)
    , m_searchStartTime(0)
    , m_running(false) {
    m_target.x = 0;
    m_target.y = 0;
    m_target.width = 0;
    m_target.height = 0;
    m_target.confidence = 0;
}

TrackingEngine::~TrackingEngine() {
    stop();
}

bool TrackingEngine::begin() {
    if (m_running) return true;

    PIDGains gainsX(m_config.kp, m_config.ki, m_config.kd);
    PIDGains gainsY(m_config.kp, m_config.ki, m_config.kd);

    m_pidX.setGains(gainsX);
    m_pidY.setGains(gainsY);

    m_pidX.setOutputLimits(-m_config.maxSpeed, m_config.maxSpeed);
    m_pidY.setOutputLimits(-m_config.maxSpeed, m_config.maxSpeed);

    m_running = true;
    s_instance = this;
    return true;
}

void TrackingEngine::update() {
    if (!m_running) return;

    unsigned long now = millis();
    float dt = 1.0f / 50.0f;

    switch (m_state) {
        case TrackState::Idle:
            break;

        case TrackState::Tracking: {
            Detection detections[8];
            int count = detectionEngine.getAllDetections(detections, 8);

            bool found = false;
            for (int i = 0; i < count; i++) {
                if (strcmp(detections[i].label, m_target.label) == 0 &&
                    fabs(detections[i].x - m_target.x) < 0.3f) {
                    m_target = detections[i];
                    m_lastSeenTime = now;
                    found = true;
                    break;
                }
            }

            if (found) {
                updateTracking(dt);
            } else if (now - m_lastSeenTime > (unsigned long)m_config.targetLostTimeoutMs) {
                m_state = TrackState::TargetLost;
                m_searchStartTime = now;
            }
            break;
        }

        case TrackState::TargetLost: {
            motionEngine.stopAxis(0);

            if (now - m_searchStartTime > (unsigned long)m_config.searchHomeDelayMs) {
                m_state = TrackState::Searching;
                m_searchStartTime = now;
                m_pidX.reset();
                m_pidY.reset();
            }

            Detection detections[8];
            int count = detectionEngine.getAllDetections(detections, 8);
            for (int i = 0; i < count; i++) {
                if (strcmp(detections[i].label, m_target.label) == 0) {
                    m_target = detections[i];
                    m_lastSeenTime = now;
                    m_state = TrackState::Tracking;
                    break;
                }
            }
            break;
        }

        case TrackState::Searching:
            updateSearching(dt);

            Detection detections[8];
            int count = detectionEngine.getAllDetections(detections, 8);
            for (int i = 0; i < count; i++) {
                if (strcmp(detections[i].label, m_target.label) == 0) {
                    m_target = detections[i];
                    m_lastSeenTime = now;
                    m_state = TrackState::Tracking;
                    break;
                }
            }
            break;
    }
}

void TrackingEngine::updateTracking(float dt) {
    float targetCenterX = m_target.x + m_target.width / 2.0f;
    float errorX = targetCenterX - 0.5f;

    float correction = m_pidX.compute(errorX, dt);
    m_correctionX = correction;

    if (fabs(errorX) < m_config.deadZone) {
        motionEngine.stopAxis(0);
        return;
    }

    long steps = (long)(correction * 100);
    motionEngine.moveRelative(0, steps);
}

void TrackingEngine::updateSearching(float dt) {
    static int searchDir = 1;
    motionEngine.moveRelative(0, 50 * searchDir);
    searchDir = -searchDir;
}

bool TrackingEngine::stop() {
    m_hasTarget = false;
    m_state = TrackState::Idle;
    m_running = false;
    s_instance = nullptr;
    return true;
}

bool TrackingEngine::setTarget(const Detection& detection) {
    m_target = detection;
    m_hasTarget = true;
    m_lastSeenTime = millis();
    m_pidX.reset();
    m_pidY.reset();
    m_state = TrackState::Tracking;
    return true;
}

bool TrackingEngine::clearTarget() {
    m_hasTarget = false;
    m_state = TrackState::Idle;
    m_pidX.reset();
    m_pidY.reset();
    return true;
}

bool TrackingEngine::isTargetLocked() const {
    return m_state == TrackState::Tracking;
}

float TrackingEngine::getCorrectionAngle() const {
    return m_correctionX;
}

bool TrackingEngine::setConfig(const TrackingConfig& config) {
    m_config = config;

    PIDGains gainsX(config.kp, config.ki, config.kd);
    PIDGains gainsY(config.kp, config.ki, config.kd);
    m_pidX.setGains(gainsX);
    m_pidY.setGains(gainsY);
    m_pidX.setOutputLimits(-config.maxSpeed, config.maxSpeed);
    m_pidY.setOutputLimits(-config.maxSpeed, config.maxSpeed);

    return true;
}

TrackingConfig TrackingEngine::getConfig() const {
    return m_config;
}

bool TrackingEngine::setStrategy(ITrackingStrategy* strategy) {
    (void)strategy;
    return true;
}

int TrackingEngine::getTargetX() const { return (int)(m_target.x * 100); }
int TrackingEngine::getTargetY() const { return (int)(m_target.y * 100); }
int TrackingEngine::getTargetWidth() const { return (int)(m_target.width * 100); }
int TrackingEngine::getTargetHeight() const { return (int)(m_target.height * 100); }
float TrackingEngine::getTargetConfidence() const { return m_target.confidence; }
