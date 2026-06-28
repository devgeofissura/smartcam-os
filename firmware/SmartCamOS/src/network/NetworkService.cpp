#include "NetworkService.h"
#include "../logger/LoggerService.h"
#include <WiFi.h>
#include <DNSServer.h>

static NetworkService* s_instance = nullptr;

static DNSServer* s_dnsServer = nullptr;

static const byte DNS_PORT = 53;

const char* NetworkService::PREFS_NAMESPACE = "smartcam-net";
const char* NetworkService::PREFS_SSID_KEY = "wifi_ssid";
const char* NetworkService::PREFS_PASS_KEY = "wifi_pass";

extern LoggerService loggerService;

NetworkService::NetworkService()
    : m_mode(WiFiMode::Station)
    , m_running(false)
    , m_apActive(false)
    , m_portalActive(false)
    , m_fallback(false)
    , m_lastTick(0) {}

NetworkService::~NetworkService() {
    deinit();
}

bool NetworkService::init() {
    if (m_running) return true;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(m_config.hostname);
    // Keep STA mode with radio initialized (not connected) so LWIP is up
    // for WebServer. Actual connection attempt happens in connectOrFallback().
    m_running = true;
    m_lastTick = millis();
    s_instance = this;
    return true;
}

void NetworkService::tick() {
    if (!m_running) return;
    if (s_dnsServer && m_portalActive) {
        s_dnsServer->processNextRequest();
    }
}

bool NetworkService::deinit() {
    if (!m_running) return true;
    disconnect();
    stopDNSServer();
    WiFi.mode(WIFI_OFF);
    m_running = false;
    s_instance = nullptr;
    return true;
}

bool NetworkService::isRunning() const {
    return m_running;
}

bool NetworkService::connect(const NetworkConfig& config) {
    if (!m_running) return false;

    m_config = config;
    stopDNSServer();

    if (strlen(config.ssid) == 0) return false;

    WiFi.setHostname(config.hostname);
    WiFi.mode(WIFI_STA);

    if (config.dhcpEnabled) {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    } else {
        IPAddress ip, gw, subnet;
        if (ip.fromString(config.staticIp) &&
            gw.fromString(config.gateway) &&
            subnet.fromString(config.subnet)) {
            WiFi.config(ip, gw, subnet);
        }
    }

    WiFi.begin(config.ssid, config.password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        attempts++;
    }

    m_mode = WiFiMode::Station;
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkService::connectOrFallback(unsigned long timeoutMs) {
    if (!m_running) return false;

    char savedSSID[32] = "";
    char savedPass[64] = "";
    bool hasCreds = loadCredentials(savedSSID, sizeof(savedSSID), savedPass, sizeof(savedPass));

    if (hasCreds && strlen(savedSSID) > 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Connecting to %s...", savedSSID);
        loggerService.info("Network", msg);
        NetworkConfig cfg;
        strncpy(cfg.ssid, savedSSID, sizeof(cfg.ssid) - 1);
        strncpy(cfg.password, savedPass, sizeof(cfg.password) - 1);
        strncpy(cfg.hostname, m_config.hostname, sizeof(cfg.hostname) - 1);

        WiFi.setHostname(cfg.hostname);
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.ssid, cfg.password);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            m_mode = WiFiMode::Station;
            m_fallback = false;
            char ipStr[16];
            snprintf(ipStr, sizeof(ipStr), "Connected, IP: %s", WiFi.localIP().toString().c_str());
            loggerService.info("Network", ipStr);
            return true;
        }

        loggerService.warning("Network", "Connection failed, starting AP");
    } else {
        loggerService.info("Network", "No saved credentials, starting AP");
    }

    m_fallback = true;
    startCaptivePortal();
    loggerService.info("Network", "AP active at 192.168.4.1 (GeoFissura)");
    return false;
}

bool NetworkService::saveCredentials(const char* ssid, const char* password) {
    if (!ssid) return false;
    m_prefs.begin(PREFS_NAMESPACE, false);
    m_prefs.putString(PREFS_SSID_KEY, ssid);
    m_prefs.putString(PREFS_PASS_KEY, password ? password : "");
    m_prefs.end();
    return true;
}

bool NetworkService::loadCredentials(char* ssid, size_t ssidMax, char* password, size_t passMax) {
    if (!ssid || ssidMax == 0) return false;
    m_prefs.begin(PREFS_NAMESPACE, true);
    String s = m_prefs.getString(PREFS_SSID_KEY, "");
    String p = m_prefs.getString(PREFS_PASS_KEY, "");
    m_prefs.end();
    strncpy(ssid, s.c_str(), ssidMax - 1);
    ssid[ssidMax - 1] = '\0';
    if (password && passMax > 0) {
        strncpy(password, p.c_str(), passMax - 1);
        password[passMax - 1] = '\0';
    }
    return strlen(ssid) > 0;
}

bool NetworkService::clearCredentials() {
    m_prefs.begin(PREFS_NAMESPACE, false);
    m_prefs.remove(PREFS_SSID_KEY);
    m_prefs.remove(PREFS_PASS_KEY);
    m_prefs.end();
    return true;
}

bool NetworkService::disconnect() {
    stopDNSServer();
    WiFi.disconnect(true);
    m_apActive = false;
    m_portalActive = false;
    return true;
}

bool NetworkService::setMode(WiFiMode mode) {
    switch (mode) {
        case WiFiMode::Station:
            WiFi.mode(WIFI_STA);
            break;
        case WiFiMode::AccessPoint:
            WiFi.mode(WIFI_AP);
            break;
        case WiFiMode::StationAP:
            WiFi.mode(WIFI_AP_STA);
            break;
    }
    m_mode = mode;
    return true;
}

bool NetworkService::startAccessPoint(const char* ssid, const char* password) {
    if (!m_running || !ssid) return false;

    WiFi.mode(WIFI_AP);

    if (password && strlen(password) >= 8) {
        WiFi.softAP(ssid, password, m_config.apChannel);
    } else {
        WiFi.softAP(ssid, nullptr, m_config.apChannel);
    }

    m_apActive = true;
    return true;
}

bool NetworkService::startCaptivePortal() {
    if (!m_running) return false;

    if (!m_apActive) {
        if (!startAccessPoint("GeoFissura", "Tenta&70")) return false;
    }

    if (!s_dnsServer) {
        s_dnsServer = new DNSServer();
    }

    s_dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
    m_portalActive = true;
    return true;
}

bool NetworkService::scanNetworks() {
    if (!m_running) return false;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int count = WiFi.scanNetworks();
    return count >= 0;
}

int NetworkService::getRssi() const {
    if (WiFi.status() != WL_CONNECTED) return 0;
    return WiFi.RSSI();
}

IPAddress NetworkService::getIp() const {
    if (m_mode == WiFiMode::AccessPoint || m_mode == WiFiMode::StationAP) {
        return WiFi.softAPIP();
    }
    return WiFi.localIP();
}

bool NetworkService::isConnected() const {
    return WiFi.status() == WL_CONNECTED || m_apActive;
}

NetworkConfig NetworkService::getConfig() const {
    return m_config;
}

void NetworkService::startDNSServer() {
    if (!s_dnsServer) {
        s_dnsServer = new DNSServer();
    }
}

void NetworkService::stopDNSServer() {
    if (s_dnsServer) {
        s_dnsServer->stop();
        delete s_dnsServer;
        s_dnsServer = nullptr;
    }
    m_portalActive = false;
}
