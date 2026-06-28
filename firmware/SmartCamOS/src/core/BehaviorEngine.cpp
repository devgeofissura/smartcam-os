#include "BehaviorEngine.h"
#include <string.h>

BehaviorEngine::BehaviorEngine() {}
BehaviorEngine::~BehaviorEngine() { stop(); }

bool BehaviorEngine::begin() { return true; }
void BehaviorEngine::update() {}
bool BehaviorEngine::stop() { return true; }

bool BehaviorEngine::registerApp(const char* name, SmartCamApp* app) {
    (void)name; (void)app;
    return true;
}

bool BehaviorEngine::unregisterApp(const char* name) {
    (void)name;
    return true;
}

bool BehaviorEngine::activateApp(const char* name) {
    (void)name;
    return true;
}

bool BehaviorEngine::deactivateApp(const char* name) {
    (void)name;
    return true;
}

bool BehaviorEngine::isAppActive(const char* name) const {
    (void)name;
    return false;
}

SmartCamApp* BehaviorEngine::getApp(const char* name) {
    (void)name;
    return nullptr;
}

int BehaviorEngine::getActiveApps(SmartCamApp** out, int maxCount) const {
    (void)out; (void)maxCount;
    return 0;
}
