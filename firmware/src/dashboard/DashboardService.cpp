#include "DashboardService.h"
#include "../api/ApiServer.h"
#include "../network/NetworkService.h"
#include "../logger/LoggerService.h"
#include "../core/camera/CameraEngine.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_camera.h>

static DashboardService* s_instance = nullptr;

extern ApiServer apiServer;
extern NetworkService networkService;
extern LoggerService loggerService;
extern CameraEngine cameraEngine;

// ============================================================
// Embedded Web Files (PROGMEM)
// ============================================================

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SmartCam Dashboard</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<div id="app">
<header>
<h1>SmartCam Dashboard</h1>
<div id="connection-status" class="disconnected">Connecting...</div>
</header>
<nav id="main-nav">
<a href="#" data-page="dashboard" class="active">Dashboard</a>
<a href="#" data-page="camera">Camera</a>
<a href="#" data-page="motion">Motion</a>
<a href="#" data-page="tracking">Tracking</a>
<a href="#" data-page="settings">Settings</a>
</nav>
<main id="content">
<div id="page-dashboard">
<div class="grid">
<div class="card"><h3>System</h3><div id="sys-info"><p>Loading...</p></div></div>
<div class="card"><h3>Memory</h3><div id="mem-info"><p>Loading...</p></div></div>
<div class="card"><h3>Network</h3><div id="net-info"><p>Loading...</p></div></div>
<div class="card"><h3>Camera</h3><div id="cam-info"><p>No camera data</p></div></div>
</div>
</div>
<div id="page-camera" style="display:none">
<div class="card">
<h3>Live Stream</h3>
<img id="cam-stream" src="" style="width:100%;max-width:640px;border-radius:4px">
</div>
</div>
<div id="page-motion" style="display:none"><div class="card"><h3>Motion Control</h3><p>Coming soon.</p></div></div>
<div id="page-tracking" style="display:none"><div class="card"><h3>Target Tracking</h3><p>Coming soon.</p></div></div>
<div id="page-settings" style="display:none"><div class="card"><h3>Settings</h3><p>Coming soon.</p></div></div>
</main>
</div>
<script src="/app.js"></script>
</body>
</html>
)rawliteral";

static const char STYLE_CSS[] PROGMEM = R"rawliteral(
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#1a1a2e;color:#e0e0e0;line-height:1.6}
header{background:#16213e;padding:1rem 2rem;display:flex;justify-content:space-between;align-items:center;border-bottom:2px solid #e94560}
#connection-status{padding:.25rem .75rem;border-radius:4px;font-size:.875rem}
#connection-status.connected{background:#4caf50}
#connection-status.disconnected{background:#f44336}
#connection-status.connecting{background:#ff9800}
#main-nav{background:#16213e;padding:.5rem 2rem;display:flex;gap:1rem}
#main-nav a{color:#a0a0a0;text-decoration:none;padding:.5rem 1rem;border-radius:4px;transition:background .2s,color .2s}
#main-nav a:hover,#main-nav a.active{background:#0f3460;color:#e0e0e0}
#content{flex:1;padding:2rem}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:1rem}
.card{background:#0f3460;border-radius:8px;padding:1.5rem;box-shadow:0 2px 8px rgba(0,0,0,.3)}
.card h3{margin-bottom:1rem;color:#e94560;font-size:1.1rem}
.card p{margin:.5rem 0;font-size:.95rem}
.card .label{color:#a0a0a0}
.card .value{float:right;font-weight:600}
table{width:100%;border-collapse:collapse}
td{padding:.4rem 0}
)rawliteral";

static const char APP_JS[] PROGMEM = R"rawliteral(
const API = window.location.origin;
let ws = null;

function updateStatus(connected) {
const el = document.getElementById('connection-status');
if (connected) { el.textContent='Connected'; el.className='connected'; }
else { el.textContent='Disconnected'; el.className='disconnected'; }
}

async function loadSystemInfo() {
try {
const r = await fetch(API + '/system');
const d = await r.json();
document.getElementById('sys-info').innerHTML =
'<table><tr><td>Uptime</td><td class="value">'+d.uptime+'s</td></tr>'+
'<tr><td>CPU Freq</td><td class="value">'+d.cpuFreq+' MHz</td></tr>'+
'<tr><td>Sketch Size</td><td class="value">'+d.sketchSize+' bytes</td></tr>'+
'<tr><td>Free Sketch</td><td class="value">'+d.freeSketch+' bytes</td></tr></table>';
document.getElementById('mem-info').innerHTML =
'<table><tr><td>Free Heap</td><td class="value">'+d.freeHeap+' bytes</td></tr>'+
'<tr><td>Min Heap</td><td class="value">'+d.minHeap+' bytes</td></tr>'+
'<tr><td>Free PSRAM</td><td class="value">'+d.freePsram+' bytes</td></tr>'+
'<tr><td>Total PSRAM</td><td class="value">'+d.totalPsram+' bytes</td></tr></table>';
} catch(e) {
document.getElementById('sys-info').innerHTML='<p>Error loading system info</p>';
document.getElementById('mem-info').innerHTML='<p>Error loading memory info</p>';
}
}

async function loadNetworkInfo() {
try {
const r = await fetch(API + '/network');
const d = await r.json();
document.getElementById('net-info').innerHTML =
'<table><tr><td>IP</td><td class="value">'+d.ip+'</td></tr>'+
'<tr><td>SSID</td><td class="value">'+d.ssid+'</td></tr>'+
'<tr><td>RSSI</td><td class="value">'+d.rssi+' dBm</td></tr>'+
'<tr><td>MAC</td><td class="value">'+d.mac+'</td></tr></table>';
updateStatus(true);
} catch(e) {
document.getElementById('net-info').innerHTML='<p>Not connected</p>';
updateStatus(false);
}
}

async function loadCameraInfo() {
try {
const r = await fetch(API + '/camera');
const d = await r.json();
document.getElementById('cam-info').innerHTML =
'<table><tr><td>Status</td><td class="value">'+d.status+'</td></tr>'+
'<tr><td>FPS</td><td class="value">'+d.fps+'</td></tr>'+
'<tr><td>Resolution</td><td class="value">'+d.resolution+'</td></tr></table>'+
'<p><a href="/camera/stream" target="_blank">Open Stream</a></p>';
} catch(e) {
document.getElementById('cam-info').innerHTML='<p>Camera not available</p>';
}
}

function navigate(page) {
document.querySelectorAll('#main-nav a').forEach(l=>l.classList.remove('active'));
document.querySelector('#main-nav a[data-page="'+page+'"]').classList.add('active');
document.querySelectorAll('#content > div').forEach(d=>d.style.display='none');
const el = document.getElementById('page-'+page);
if (el) el.style.display='block';
if (page==='camera') document.getElementById('cam-stream').src='/camera/stream';
}
document.querySelectorAll('#main-nav a').forEach(function(a){
a.addEventListener('click',function(e){
e.preventDefault(); navigate(this.dataset.page);
});
});

function refresh() { loadSystemInfo(); loadNetworkInfo(); loadCameraInfo(); }
setInterval(refresh, 3000);
document.addEventListener('DOMContentLoaded', refresh);
)rawliteral";

// ============================================================
// Dashboard Service
// ============================================================

DashboardService::DashboardService()
    : m_running(false) {}

DashboardService::~DashboardService() {
    deinit();
}

bool DashboardService::init() {
    if (m_running) return true;

    apiServer.start(80);
    registerRoutes();

    m_running = true;
    s_instance = this;
    return true;
}

void DashboardService::tick() {}

bool DashboardService::deinit() {
    m_running = false;
    s_instance = nullptr;
    return true;
}

bool DashboardService::isRunning() const {
    return m_running;
}

static void handleRootRoute() {
    if (s_instance) s_instance->handleRoot();
}

static void handleCssRoute() {
    if (s_instance) s_instance->handleCss();
}

static void handleJsRoute() {
    if (s_instance) s_instance->handleJs();
}

static void handleSystemInfoRoute() {
    if (s_instance) s_instance->handleSystemInfo();
}

static void handleNetworkInfoRoute() {
    if (s_instance) s_instance->handleNetworkInfo();
}

static void handleLoggerRoute() {
    if (s_instance) s_instance->handleLogger();
}

static void handleCameraInfoRoute() {
    if (s_instance) s_instance->handleCameraInfo();
}

static void handleCameraStreamRoute() {
    if (s_instance) s_instance->handleCameraStream();
}

static void handleApiInfoRoute() {
    if (s_instance) s_instance->handleApiInfo();
}

void DashboardService::registerRoutes() {
    apiServer.registerEndpoint("GET", "/", handleRootRoute);
    apiServer.registerEndpoint("GET", "/index.html", handleRootRoute);
    apiServer.registerEndpoint("GET", "/style.css", handleCssRoute);
    apiServer.registerEndpoint("GET", "/app.js", handleJsRoute);
    apiServer.registerEndpoint("GET", "/system", handleSystemInfoRoute);
    apiServer.registerEndpoint("GET", "/network", handleNetworkInfoRoute);
    apiServer.registerEndpoint("GET", "/logger", handleLoggerRoute);
    apiServer.registerEndpoint("GET", "/camera", handleCameraInfoRoute);
    apiServer.registerEndpoint("GET", "/camera/stream", handleCameraStreamRoute);
    apiServer.registerEndpoint("GET", "/api/info", handleApiInfoRoute);
}

void DashboardService::handleRoot() {
    size_t len = strlen_P(INDEX_HTML);
    char buf[len + 1];
    strcpy_P(buf, INDEX_HTML);
    apiServer.sendResponse(200, "text/html", buf);
}

void DashboardService::handleCss() {
    size_t len = strlen_P(STYLE_CSS);
    char buf[len + 1];
    strcpy_P(buf, STYLE_CSS);
    apiServer.sendResponse(200, "text/css", buf);
}

void DashboardService::handleJs() {
    size_t len = strlen_P(APP_JS);
    char buf[len + 1];
    strcpy_P(buf, APP_JS);
    apiServer.sendResponse(200, "application/javascript", buf);
}

void DashboardService::handleNetworkInfo() {
    char buf[512];
    IPAddress ip = networkService.getIp();
    uint8_t mac[6];
    WiFi.macAddress(mac);

    snprintf(buf, sizeof(buf),
        "{"
        "\"status\":\"ok\","
        "\"ip\":\"%d.%d.%d.%d\","
        "\"ssid\":\"%s\","
        "\"rssi\":%d,"
        "\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
        "\"connected\":%s"
        "}",
        ip[0], ip[1], ip[2], ip[3],
        WiFi.SSID().c_str(),
        networkService.getRssi(),
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
        networkService.isConnected() ? "true" : "false");

    apiServer.sendJson(200, buf);
}

void DashboardService::handleLogger() {
    char buf[2048];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "{\"status\":\"ok\",\"level\":%d,\"count\":%d,\"entries\":[",
        (int)loggerService.getLevel(), loggerService.getEntryCount());

    LogEntry entries[32];
    int count = 0;
    loggerService.getEntries(entries, 32, count);

    for (int i = 0; i < count && pos < (int)sizeof(buf) - 128; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"t\":%lu,\"l\":%d,\"m\":\"%s\",\"msg\":\"%s\"}",
            entries[i].timestamp, (int)entries[i].level,
            entries[i].module, entries[i].message);
    }

    snprintf(buf + pos, sizeof(buf) - pos, "]}");
    apiServer.sendJson(200, buf);
}

void DashboardService::handleCameraInfo() {
    char buf[256];
    if (cameraEngine.isInitialized()) {
        const CameraFrame* frame = cameraEngine.getCurrentFrame();
        snprintf(buf, sizeof(buf),
            "{"
            "\"status\":\"ok\","
            "\"fps\":%d,"
            "\"resolution\":\"%dx%d\","
            "\"streaming\":%s"
            "}",
            cameraEngine.getFps(),
            frame ? frame->width : 0,
            frame ? frame->height : 0,
            cameraEngine.isStreaming() ? "true" : "false");
    } else {
        snprintf(buf, sizeof(buf),
            "{\"status\":\"unavailable\",\"fps\":0,\"resolution\":\"-\"}");
    }
    apiServer.sendJson(200, buf);
}

void DashboardService::handleCameraStream() {
    if (!cameraEngine.isInitialized()) {
        apiServer.sendError(503, "Camera not initialized");
        return;
    }

    const char* boundary = "frame";
    char header[128];
    snprintf(header, sizeof(header),
        "multipart/x-mixed-replace;boundary=%s", boundary);

    if (!apiServer.beginStream(header)) return;

    unsigned long startTime = millis();
    while (millis() - startTime < 60000) { // 60s max stream
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            delay(10);
            continue;
        }

        char partHeader[128];
        int phLen = snprintf(partHeader, sizeof(partHeader),
            "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            boundary, fb->len);

        apiServer.streamChunk((uint8_t*)partHeader, phLen);
        apiServer.streamChunk(fb->buf, fb->len);
        esp_camera_fb_return(fb);
    }

    char trailer[32];
    snprintf(trailer, sizeof(trailer), "\r\n--%s--\r\n", boundary);
    apiServer.streamChunk((uint8_t*)trailer, strlen(trailer));
    apiServer.endStream();
}

void DashboardService::handleApiInfo() {
    apiServer.sendJson(200,
        "{\"status\":\"ok\",\"endpoints\":["
        "\"/\",\"/index.html\",\"/style.css\",\"/app.js\","
        "\"/system\",\"/network\",\"/logger\",\"/camera\","
        "\"/camera/stream\",\"/api/info\""
        "]}");
}

void DashboardService::handleSystemInfo() {
    char buf[512];
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t freePsram = ESP.getPsramSize() - ESP.getPsramFree();
    uint32_t totalPsram = ESP.getPsramSize();

    snprintf(buf, sizeof(buf),
        "{"
        "\"status\":\"ok\","
        "\"uptime\":%lu,"
        "\"cpuFreq\":%d,"
        "\"freeHeap\":%u,"
        "\"minHeap\":%u,"
        "\"freePsram\":%u,"
        "\"totalPsram\":%u,"
        "\"sketchSize\":%u,"
        "\"freeSketch\":%u"
        "}",
        millis() / 1000,
        getCpuFrequencyMhz(),
        freeHeap,
        ESP.getMinFreeHeap(),
        freePsram,
        totalPsram,
        ESP.getSketchSize(),
        ESP.getFreeSketch());

    apiServer.sendJson(200, buf);
}
