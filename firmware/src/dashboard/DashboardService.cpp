#include "DashboardService.h"
#include "../api/ApiServer.h"
#include "../network/NetworkService.h"
#include <WiFi.h>
#include <esp_heap_caps.h>

static DashboardService* s_instance = nullptr;

extern ApiServer apiServer;
extern NetworkService networkService;

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
'<tr><td>Resolution</td><td class="value">'+d.resolution+'</td></tr></table>';
} catch(e) {
document.getElementById('cam-info').innerHTML='<p>Camera not available</p>';
}
}

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

void DashboardService::registerRoutes() {
    apiServer.registerEndpoint("GET", "/", handleRootRoute);
    apiServer.registerEndpoint("GET", "/index.html", handleRootRoute);
    apiServer.registerEndpoint("GET", "/style.css", handleCssRoute);
    apiServer.registerEndpoint("GET", "/app.js", handleJsRoute);
    apiServer.registerEndpoint("GET", "/system", handleSystemInfoRoute);
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
