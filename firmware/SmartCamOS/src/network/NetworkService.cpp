#include "NetworkService.h"
#include <WiFi.h>
#include <DNSServer.h>

static NetworkService* s_instance = nullptr;

static DNSServer* s_dnsServer = nullptr;

static const byte DNS_PORT = 53;

NetworkService::NetworkService()
    : m_mode(WiFiMode::Station)
    , m_running(false)
    , m_apActive(false)
    , m_portalActive(false)
    , m_lastTick(0) {}

NetworkService::~NetworkService() {
    deinit();
}

bool NetworkService::init() {
    if (m_running) return true;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(m_config.hostname);
    WiFi.mode(WIFI_OFF);
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
        if (!startAccessPoint("SmartCamOS", nullptr)) return false;
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
