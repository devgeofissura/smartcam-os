/*
 * SmartCam OS — Embedded Computer Vision Platform
 * Hardware: LilyGO T-SIMCAM ESP32-S3 V1.6
 * Framework: Arduino ESP32
 *
 * This is the entry point for the SmartCam Platform firmware.
 * See docs/ for the complete architecture documentation.
 *
 * Sprint 1: Core interfaces and base classes.
 * Sprint 2: Logger Service — first fully implemented module.
 * Sprint 3: Configuration Manager — JSON persistence, profiles, schema.
 * Sprint 4: Network Service — Wi-Fi STA/AP, captive portal, DNS.
 * Sprint 5: Dashboard Service — web interface served from PROGMEM.
 * Sprint 6: REST API — /network, /logger, /camera, /api/info endpoints.
 * Sprint 7: Camera Engine — OV2640 init, capture, JPEG config.
 * Sprint 8: MJPEG Streaming — /camera/stream endpoint.
 * Sprint 9: Motion Engine — DM556D driver, STEP/DIR, HOME, STOP.
 * Sprint 10: Camera × Motor integration — /motion API, web controls.
 * Sprint 11: Vision Engine — gray, HSV, threshold, blob detection.
 * Sprint 12: Detection Engine — color detector (first detector).
 * Sprint 13: Tracking Engine — PID centering, motor follows target.
 */

#include <Arduino.h>

// ============================================================
// SDK Interfaces
// ============================================================
#include "../sdk/interfaces/SmartCamObject.h"
#include "../sdk/interfaces/SmartCamModule.h"
#include "../sdk/interfaces/SmartCamService.h"
#include "../sdk/interfaces/SmartCamApp.h"
#include "../sdk/interfaces/Event.h"
#include "../sdk/interfaces/IEventListener.h"
#include "../sdk/interfaces/IDetector.h"
#include "../sdk/interfaces/IMotionController.h"
#include "../sdk/interfaces/IFrameProcessor.h"
#include "../sdk/interfaces/ITrackingStrategy.h"

// ============================================================
// SDK Core
// ============================================================
#include "../sdk/core/EventBus.h"
#include "../sdk/core/SmartCamAPI.h"

// ============================================================
// Firmware Utils
// ============================================================
#include "src/utils/Result.h"
#include "src/utils/SystemState.h"
#include "src/utils/CoreAffinity.h"

// ============================================================
// Core Engines
// ============================================================
#include "src/core/camera/CameraEngine.h"
#include "src/core/vision/VisionEngine.h"
#include "src/core/tracking/TrackingEngine.h"
#include "src/core/motion/MotionEngine.h"
#include "src/core/ai/AIEngine.h"
#include "src/core/ai/ColorDetector.h"
#include "src/core/BehaviorEngine.h"

// ============================================================
// Infrastructure Services
// ============================================================
#include "src/network/NetworkService.h"
#include "src/storage/StorageService.h"
#include "src/logger/LoggerService.h"
#include "src/api/ApiServer.h"
#include "src/config/ConfigManager.h"
#include "src/dashboard/DashboardService.h"

// ============================================================
// Global Instances
// ============================================================

// Core Engines
CameraEngine    cameraEngine;
VisionEngine    visionEngine;
TrackingEngine  trackingEngine;
MotionEngine    motionEngine;
DetectionEngine detectionEngine;
BehaviorEngine  behaviorEngine;

// Infrastructure Services
NetworkService      networkService;
StorageService      storageService;
LoggerService       loggerService;
ApiServer           apiServer;
ConfigManager       configManager;
DashboardService    dashboardService;

// System State
SystemState g_systemState = SystemState::Boot;

// ============================================================
// Forward Declarations
// ============================================================

void setupCore();
void setupCamera();
void setupMotion();
void setupVision();
void setupTracking();
void setupAI();
void setupBehavior();
void setupNetwork();
void setupStorage();
void setupLogger();
void setupAPI();
void setupConfig();
void setupDashboard();

void loopCore();
void loopCamera();
void loopMotion();
void loopVision();
void loopTracking();
void loopAI();
void loopBehavior();
void loopNetwork();
void loopStorage();
void loopLogger();
void loopAPI();
void loopConfig();
void loopDashboard();

// ============================================================
// Initialization
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println(F("SmartCam OS v0.13.0 Sprint 13 - Tracking Engine"));
    Serial.println(F("Platform: ESP32-S3 / T-SIMCAM v1.6"));

    g_systemState = SystemState::Init;

    setupLogger();
    loggerService.info("System", "Logger service ready");

    setupStorage();
    setupConfig();
    setupCore();
    setupNetwork();
    setupCamera();
    setupVision();
    setupMotion();
    setupTracking();
    setupAI();
    setupBehavior();
    setupAPI();
    setupDashboard();

    g_systemState = SystemState::Ready;
    loggerService.info("System", "SmartCam OS ready — all systems initialized");
}

// ============================================================
// Main Loop
// ============================================================

void loop() {
    loopLogger();
    loopStorage();
    loopConfig();
    loopCore();
    loopNetwork();
    loopCamera();
    loopVision();
    loopMotion();
    loopTracking();
    loopAI();
    loopBehavior();
    loopAPI();
    loopDashboard();
}

// ============================================================
// Stub Implementations (replaced as each Sprint implements the real module) — interfaces only)
// ============================================================

void setupCore()     { /* Core scheduler and state manager - Sprint 3 */ } — Sprint 2 */ }
void setupCamera()   {
    cameraEngine.setPins(CameraPins());
    if (cameraEngine.begin()) {
        cameraEngine.startStream();
        loggerService.info("Camera", "Camera initialized and streaming");
    } else {
        loggerService.warning("Camera", "Camera initialization failed");
    }
}
void setupMotion()   {
    motionEngine.begin();
    AxisConfig panAxis;
    panAxis.stepPin = 39;
    panAxis.dirPin = 40;
    panAxis.enablePin = 41;
    panAxis.homePin = -1;
    panAxis.stepsPerDegree = 16.0f * 200.0f / 360.0f;
    panAxis.maxSpeed = 5000.0f;
    panAxis.acceleration = 1000.0f;
    panAxis.microSteps = 16;
    if (motionEngine.addAxis(panAxis)) {
        motionEngine.enableAxis(0, true);
        loggerService.info("Motion", "Pan axis initialized");
    } else {
        loggerService.warning("Motion", "Pan axis init failed");
    }
}
void setupVision()   {
    if (visionEngine.begin()) {
        loggerService.info("Vision", "Vision engine initialized");
    } else {
        loggerService.warning("Vision", "Vision engine init failed");
    }
}
void setupTracking() {
    trackingEngine.begin();
    Detection initialTarget;
    initialTarget.x = 0.5f;
    initialTarget.y = 0.5f;
    initialTarget.width = 0.1f;
    initialTarget.height = 0.1f;
    initialTarget.confidence = 0.0f;
    initialTarget.classId = 0;
    strcpy(initialTarget.label, "red");
    trackingEngine.setTarget(initialTarget);
    loggerService.info("Tracking", "Tracking engine started");
}
void setupAI()       {
    detectionEngine.begin();

    ColorDetectorConfig redConfig;
    redConfig.range.hMin = 0; redConfig.range.hMax = 20;
    redConfig.range.sMin = 50; redConfig.range.sMax = 255;
    redConfig.range.vMin = 50; redConfig.range.vMax = 255;
    redConfig.minBlobArea = 100;
    redConfig.label = "red";

    ColorDetector* redDetector = new ColorDetector(redConfig);
    if (redDetector && detectionEngine.registerDetector("red", redDetector)) {
        detectionEngine.setActiveDetector("red");
        loggerService.info("AI", "Red color detector registered");
    } else {
        delete redDetector;
        loggerService.warning("AI", "Red color detector registration failed");
    }
}
void setupBehavior() { behaviorEngine.begin(); }
void setupNetwork()  { networkService.init(); }
void setupStorage()  { storageService.init(); }
void setupLogger()   { loggerService.init(); }
void setupAPI()      { apiServer.init(); }
void setupConfig()   { configManager.init(); }
void setupDashboard(){ dashboardService.init(); }

void loopCore()     { delay(5); }
void loopCamera()   { cameraEngine.update(); }
void loopMotion()   { motionEngine.update(); }
void loopVision()   { visionEngine.update(); }
void loopTracking() { trackingEngine.update(); }
void loopAI()       { detectionEngine.update(); }
void loopBehavior() { behaviorEngine.update(); }
void loopNetwork()  { networkService.tick(); }
void loopStorage()  { storageService.tick(); }
void loopLogger()   { loggerService.tick(); }
void loopAPI()      { apiServer.tick(); }
void loopConfig()   { configManager.tick(); }
void loopDashboard(){ dashboardService.tick(); }
