#ifndef SMARTCAM_DASHBOARD_SERVICE_H
#define SMARTCAM_DASHBOARD_SERVICE_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamService.h"

class DashboardService : public SmartCamService {
public:
    bool init() override;
    void tick() override;
    bool deinit() override;
    bool isRunning() const override;

    bool servePage(const char* page);
    bool registerPage(const char* route, const char* filePath);
    bool setSystemInfo(const char* key, const char* value);
    bool broadcastEvent(const char* eventType, const char* jsonData);

    const char* name() const override { return "DashboardService"; }

private:
    bool m_running;

    void registerRoutes();
    void handleRoot();
    void handleCss();
    void handleJs();
    void handleSystemInfo();
    void handleNetworkInfo();
    void handleLogger();
    void handleCameraInfo();
    void handleCameraStream();
    void handleMotionInfo();
    void handleMotionCommand();
    void handleVisionInfo();
    void handleDetectionInfo();
    void handleTrackingInfo();
    void handleTrackingCommand();
    void handleApiInfo();

public:
    DashboardService();
    ~DashboardService();
};

#endif
