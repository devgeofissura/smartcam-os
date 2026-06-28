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
    Serial.println(F("SmartCam OS v0.5.0 Sprint 5 - Dashboard Service"));
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
void setupCamera()   { cameraEngine.begin(); }
void setupMotion()   { motionEngine.begin(); }
void setupVision()   { visionEngine.begin(); }
void setupTracking() { trackingEngine.begin(); }
void setupAI()       { detectionEngine.begin(); }
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
