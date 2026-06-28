#ifndef SMARTCAM_BEHAVIOR_ENGINE_H
#define SMARTCAM_BEHAVIOR_ENGINE_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamModule.h"
#include "../../sdk/interfaces/SmartCamApp.h"

class BehaviorEngine : public SmartCamModule {
public:
    bool begin() override;
    void update() override;
    bool stop() override;

    bool registerApp(const char* name, SmartCamApp* app);
    bool unregisterApp(const char* name);
    bool activateApp(const char* name);
    bool deactivateApp(const char* name);
    bool isAppActive(const char* name) const;
    SmartCamApp* getApp(const char* name);
    int getActiveApps(SmartCamApp** out, int maxCount) const;

    const char* name() const override { return "BehaviorEngine"; }

    BehaviorEngine();
    ~BehaviorEngine();
};

#endif
