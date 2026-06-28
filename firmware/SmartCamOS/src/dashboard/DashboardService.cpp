#include "DashboardService.h"
#include "../api/ApiServer.h"
#include "../network/NetworkService.h"
#include "../logger/LoggerService.h"
#include "../core/camera/CameraEngine.h"
#include "../core/motion/MotionEngine.h"
#include "../core/vision/VisionEngine.h"
#include "../core/ai/AIEngine.h"
#include "../core/ai/ColorDetector.h"
#include "../core/tracking/TrackingEngine.h"
#include "../apps/PersonTrackerApp.h"
#include <esp_camera.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_camera.h>

static DashboardService* s_instance = nullptr;

extern ApiServer apiServer;
extern NetworkService networkService;
extern LoggerService loggerService;
extern CameraEngine cameraEngine;
extern MotionEngine motionEngine;
extern VisionEngine visionEngine;
extern DetectionEngine detectionEngine;
extern TrackingEngine trackingEngine;
extern PersonTrackerApp personTracker;

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
<div id="page-motion" style="display:none">
<div class="card"><h3>Axis Control</h3>
<p>Position: <span id="mot-pos">0</span></p>
<p>Speed: <span id="mot-speed">0</span></p>
<p>Homed: <span id="mot-homed">-</span></p>
<p>Moving: <span id="mot-moving">-</span></p>
<p>Enabled: <span id="mot-enabled">-</span></p>
<div style="margin-top:1rem">
<button onclick="sendMotion('home',0)">Home</button>
<button onclick="sendMotion('move',0,100)">+100</button>
<button onclick="sendMotion('move',0,-100)">-100</button>
<button onclick="sendMotion('stop',0)">Stop</button>
<button onclick="sendMotion('enable',0,1)">Enable</button>
<button onclick="sendMotion('enable',0,0)">Disable</button>
</div></div></div>
<div id="page-tracking" style="display:none">
  <div class="card"><h3>Object Tracking</h3>
  <p>Target: <span id="track-target">person</span> &middot; Status: <span id="track-status">idle</span></p>
  <p>Correction: <span id="track-correction">0.0000</span></p>
  <p>Target: <span id="track-pos">(0, 0)</span> &middot; Size: <span id="track-size">0x0</span> &middot; Conf: <span id="track-conf">0.00</span></p>
  <div class="button-row">
    <button onclick="fetch('/tracking',{method:'POST',body:'action=track-person'}).then(r=>r.json()).then(d=>document.getElementById('track-status').textContent='tracking person')">Track Person</button>
    <button onclick="fetch('/tracking',{method:'POST',body:'action=track-color&label=red'}).then(r=>r.json()).then(d=>document.getElementById('track-status').textContent='tracking red')">Track Red</button>
    <button onclick="fetch('/tracking',{method:'POST',body:'action=stop'})" style="background:var(--danger)">Stop</button>
  </div>
  <script>
  setInterval(function(){
    fetch('/tracking').then(r=>r.json()).then(d=>{
      document.getElementById('track-status').textContent=d.locked?'locked':'searching';
      document.getElementById('track-correction').textContent=d.correction.toFixed(4);
      document.getElementById('track-pos').textContent='('+d.tx+', '+d.ty+')';
      document.getElementById('track-size').textContent=d.tw+'x'+d.th;
      document.getElementById('track-conf').textContent=d.conf.toFixed(2);
    });
  },1000);
  </script>
  </div>
</div>
<div id="page-settings" style="display:none">
  <div class="card"><h3>WiFi Configuration</h3>
    <p>Status: <span id="wifi-status">Loading...</span></p>
    <div style="margin-top:1rem">
      <label>Select Network:</label>
      <select id="wifi-networks" style="width:100%;padding:.5rem;margin:.25rem 0 1rem;background:#16213e;color:#e0e0e0;border:1px solid #333;border-radius:4px">
        <option value="">-- Scan for networks --</option>
      </select>
      <label>Password:</label>
      <input type="password" id="wifi-password" placeholder="WPA2 password (leave blank for open)" style="width:100%;padding:.5rem;margin:.25rem 0 1rem;background:#16213e;color:#e0e0e0;border:1px solid #333;border-radius:4px">
      <button id="wifi-scan-btn" onclick="wifiScan()" style="margin-right:.5rem">Scan</button>
      <button id="wifi-connect-btn" onclick="wifiConnect()" style="background:var(--success)">Save &amp; Reboot</button>
    </div>
    <div id="wifi-msg" style="margin-top:.5rem;font-size:.9rem"></div>
  </div>
  <div class="card"><h3>System</h3>
    <button onclick="fetch('/api/wifi/status').then(r=>r.json()).then(d=>document.getElementById('wifi-status').textContent=d.mode+' ('+d.ip+')')" style="margin-right:.5rem">Refresh Status</button>
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
if (page==='settings') { wifiScan(); wifiStatus(); }
}

async function wifiStatus() {
try {
const r = await fetch('/api/wifi/status');
const d = await r.json();
document.getElementById('wifi-status').textContent = d.mode + ' (' + d.ip + (d.ssid ? ', ' + d.ssid : '') + ')';
} catch(e) {}
}
document.querySelectorAll('#main-nav a').forEach(function(a){
a.addEventListener('click',function(e){
e.preventDefault(); navigate(this.dataset.page);
});
});

async function loadMotionInfo() {
try {
const r = await fetch(API + '/motion');
const d = await r.json();
if (d.axes && d.axes.length>0) {
const a = d.axes[0];
document.getElementById('mot-pos').textContent = a.position;
document.getElementById('mot-speed').textContent = a.speed.toFixed(1);
document.getElementById('mot-homed').textContent = a.homed;
document.getElementById('mot-moving').textContent = a.moving;
document.getElementById('mot-enabled').textContent = a.enabled;
}} catch(e) {}
}

async function sendMotion(cmd, axis, val) {
const body = 'cmd='+cmd+'&axis='+axis+(val!==undefined?'&value='+val:'');
try {
await fetch(API + '/motion', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
loadMotionInfo();
} catch(e) { console.error(e); }
}

async function wifiScan() {
const btn = document.getElementById('wifi-scan-btn');
btn.textContent='Scanning...'; btn.disabled=true;
document.getElementById('wifi-msg').textContent='';
try {
const r = await fetch('/api/wifi/scan');
const d = await r.json();
const sel = document.getElementById('wifi-networks');
sel.innerHTML = '<option value="">-- Select network --</option>';
if (d.networks && d.networks.length>0) {
d.networks.forEach(function(n){
const opt = document.createElement('option');
opt.value = n.ssid;
opt.textContent = n.ssid + ' (' + n.rssi + ' dBm)' + (n.open ? ' [open]' : '');
sel.appendChild(opt);
});
document.getElementById('wifi-msg').textContent = d.networks.length + ' networks found';
} else {
document.getElementById('wifi-msg').textContent = 'No networks found';
}} catch(e) {
document.getElementById('wifi-msg').textContent = 'Scan failed';
}
btn.textContent='Scan'; btn.disabled=false;
}

async function wifiConnect() {
const ssid = document.getElementById('wifi-networks').value;
const pw = document.getElementById('wifi-password').value;
if (!ssid) { document.getElementById('wifi-msg').textContent='Select a network'; return; }
document.getElementById('wifi-msg').textContent='Saving and rebooting...';
try {
const body = 'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pw);
await fetch('/api/wifi/config', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
} catch(e) { document.getElementById('wifi-msg').textContent='Config saved, rebooting...'; }
setTimeout(function(){ window.location.reload(); }, 3000);
}

function refresh() { loadSystemInfo(); loadNetworkInfo(); loadCameraInfo(); loadMotionInfo(); }
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

static void handleMotionInfoRoute() {
    if (s_instance) s_instance->handleMotionInfo();
}

static void handleMotionCommandRoute() {
    if (s_instance) s_instance->handleMotionCommand();
}

static void handleVisionInfoRoute() {
    if (s_instance) s_instance->handleVisionInfo();
}

static void handleDetectionInfoRoute() {
    if (s_instance) s_instance->handleDetectionInfo();
}

static void handleTrackingInfoRoute() {
    if (s_instance) s_instance->handleTrackingInfo();
}

static void handleTrackingCommandRoute() {
    if (s_instance) s_instance->handleTrackingCommand();
}

static void handleApiInfoRoute() {
    if (s_instance) s_instance->handleApiInfo();
}

static void handleWifiConfigRoute() {
    if (s_instance) s_instance->handleWifiConfig();
}

static void handleWifiScanRoute() {
    if (s_instance) s_instance->handleWifiScan();
}

static void handleWifiStatusRoute() {
    if (s_instance) s_instance->handleWifiStatus();
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
    apiServer.registerEndpoint("GET", "/motion", handleMotionInfoRoute);
    apiServer.registerEndpoint("POST", "/motion", handleMotionCommandRoute);
    apiServer.registerEndpoint("GET", "/vision", handleVisionInfoRoute);
    apiServer.registerEndpoint("GET", "/detect", handleDetectionInfoRoute);
    apiServer.registerEndpoint("GET", "/tracking", handleTrackingInfoRoute);
    apiServer.registerEndpoint("POST", "/tracking", handleTrackingCommandRoute);
    apiServer.registerEndpoint("GET", "/api/info", handleApiInfoRoute);
    apiServer.registerEndpoint("POST", "/api/wifi/config", handleWifiConfigRoute);
    apiServer.registerEndpoint("GET", "/api/wifi/scan", handleWifiScanRoute);
    apiServer.registerEndpoint("GET", "/api/wifi/status", handleWifiStatusRoute);
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
        const Frame* frame = cameraEngine.getCurrentFrame();
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

void DashboardService::handleMotionInfo() {
    char buf[512];
    int count = motionEngine.getAxisCount();
    int pos = snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"axes\":[");

    for (int i = 0; i < count && pos < (int)sizeof(buf) - 128; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        AxisState s = motionEngine.getAxisState(i);
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"index\":%d,\"position\":%ld,\"speed\":%.1f,"
            "\"homed\":%s,\"moving\":%s,\"enabled\":%s}",
            i, s.currentPosition, s.currentSpeed,
            s.isHomed ? "true" : "false",
            s.isMoving ? "true" : "false",
            s.isEnabled ? "true" : "false");
    }

    snprintf(buf + pos, sizeof(buf) - pos, "]}");
    apiServer.sendJson(200, buf);
}

void DashboardService::handleMotionCommand() {
    String cmd = apiServer.getArg("cmd");
    if (cmd.length() == 0) {
        apiServer.sendError(400, "Missing cmd parameter");
        return;
    }

    int axis = atoi(apiServer.getArg("axis").c_str());

    if (cmd == "enable") {
        bool on = apiServer.getArg("value") == "1";
        apiServer.sendJson(200, motionEngine.enableAxis(axis, on)
            ? "{\"status\":\"ok\"}" : "{\"status\":\"error\"}");
    } else if (cmd == "move") {
        long steps = atol(apiServer.getArg("value").c_str());
        apiServer.sendJson(200, motionEngine.moveRelative(axis, steps)
            ? "{\"status\":\"ok\"}" : "{\"status\":\"error\"}");
    } else if (cmd == "stop") {
        apiServer.sendJson(200, motionEngine.stopAxis(axis)
            ? "{\"status\":\"ok\"}" : "{\"status\":\"error\"}");
    } else if (cmd == "home") {
        apiServer.sendJson(200, motionEngine.homeAxis(axis)
            ? "{\"status\":\"ok\"}" : "{\"status\":\"error\"}");
    } else {
        apiServer.sendError(400, "Unknown command");
    }
}

void DashboardService::handleVisionInfo() {
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"ready\":%s,\"width\":%d,\"height\":%d}",
        visionEngine.getWorkingBuffer() ? "true" : "false",
        visionEngine.getWorkingWidth(),
        visionEngine.getWorkingHeight());
    apiServer.sendJson(200, buf);
}

void DashboardService::handleDetectionInfo() {
    char buf[1024];
    Detection detections[8];
    int count = detectionEngine.getAllDetections(detections, 8);

    int pos = snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"detectors\":%d,\"count\":%d,\"detections\":[",
        detectionEngine.getDetector("red") ? 1 : 0, count);

    for (int i = 0; i < count && pos < (int)sizeof(buf) - 128; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"label\":\"%s\",\"x\":%.3f,\"y\":%.3f,"
            "\"w\":%.3f,\"h\":%.3f,\"conf\":%.2f}",
            detections[i].label,
            detections[i].x, detections[i].y,
            detections[i].width, detections[i].height,
            detections[i].confidence);
    }

    snprintf(buf + pos, sizeof(buf) - pos, "]}");
    apiServer.sendJson(200, buf);
}

void DashboardService::handleTrackingInfo() {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"locked\":%s,\"correction\":%.4f,"
        "\"tx\":%d,\"ty\":%d,\"tw\":%d,\"th\":%d,\"conf\":%.2f}",
        trackingEngine.isTargetLocked() ? "true" : "false",
        trackingEngine.getCorrectionAngle(),
        trackingEngine.getTargetX(),
        trackingEngine.getTargetY(),
        trackingEngine.getTargetWidth(),
        trackingEngine.getTargetHeight(),
        trackingEngine.getTargetConfidence());
    apiServer.sendJson(200, buf);
}

void DashboardService::handleTrackingCommand() {
    String action = apiServer.getArg("action");

    if (action == "track-person") {
        if (personTracker.startTrackingPerson()) {
            apiServer.sendJson(200, "{\"status\":\"ok\",\"message\":\"Tracking person\"}");
        } else {
            apiServer.sendJson(400, "{\"status\":\"error\",\"message\":\"Person detector unavailable\"}");
        }
    } else if (action == "track-color") {
        String colorLabel = apiServer.getArg("label");
        if (colorLabel.length() > 0 && personTracker.startTrackingColor(colorLabel.c_str())) {
            apiServer.sendJson(200, "{\"status\":\"ok\",\"message\":\"Tracking color\"}");
        } else {
            apiServer.sendJson(400, "{\"status\":\"error\",\"message\":\"Unknown color label\"}");
        }
    } else if (action == "stop") {
        personTracker.stopTracking();
        apiServer.sendJson(200, "{\"status\":\"ok\",\"message\":\"Tracking stopped\"}");
    } else if (action == "set-detector") {
        String name = apiServer.getArg("name");
        if (name.length() > 0 && personTracker.setDetector(name.c_str())) {
            apiServer.sendJson(200, "{\"status\":\"ok\",\"message\":\"Detector changed\"}");
        } else {
            apiServer.sendJson(400, "{\"status\":\"error\",\"message\":\"Unknown detector\"}");
        }
    } else {
        apiServer.sendJson(400, "{\"status\":\"error\",\"message\":\"Unknown action\"}");
    }
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
        "\"/camera/stream\",\"/motion\",\"/vision\",\"/detect\",\"/tracking\",\"/tracking\"(POST),"
        "\"/api/wifi/config\"(POST),\"/api/wifi/scan\",\"/api/wifi/status\",\"/api/info\""
        "]}");
}

void DashboardService::handleWifiConfig() {
    String ssid = apiServer.getArg("ssid");
    String password = apiServer.getArg("password");

    if (ssid.length() == 0) {
        apiServer.sendError(400, "Missing ssid parameter");
        return;
    }

    networkService.saveCredentials(ssid.c_str(), password.c_str());
    apiServer.sendJson(200, "{\"status\":\"ok\",\"message\":\"Credentials saved, rebooting...\"}");

    delay(500);
    ESP.restart();
}

void DashboardService::handleWifiScan() {
    // Switch to AP+STA so AP clients stay connected during scan
    bool wasAp = (WiFi.getMode() == WIFI_MODE_AP);
    if (wasAp) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
    int count = WiFi.scanNetworks();
    if (wasAp) {
        WiFi.mode(WIFI_AP);
    }
    char buf[1024];
    int pos = snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"networks\":[");

    for (int i = 0; i < count && pos < (int)sizeof(buf) - 64; i++) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        uint8_t encryption = WiFi.encryptionType(i);
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%s}",
            ssid.c_str(), rssi, encryption == WIFI_AUTH_OPEN ? "true" : "false");
    }

    snprintf(buf + pos, sizeof(buf) - pos, "]}");
    apiServer.sendJson(200, buf);
}

void DashboardService::handleWifiStatus() {
    char buf[256];
    char checkSSID[32] = "";
    bool hasCreds = networkService.loadCredentials(checkSSID, sizeof(checkSSID), nullptr, 0);
    if (networkService.isFallbackMode()) {
        snprintf(buf, sizeof(buf),
            "{\"status\":\"ok\",\"mode\":\"ap\",\"ip\":\"192.168.4.1\","
            "\"ssid\":\"GeoFissura\",\"connected\":false,\"configured\":%s}",
            hasCreds ? "true" : "false");
    } else if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf),
            "{\"status\":\"ok\",\"mode\":\"sta\",\"ip\":\"%s\","
            "\"ssid\":\"%s\",\"rssi\":%d,\"connected\":true}",
            WiFi.localIP().toString().c_str(),
            WiFi.SSID().c_str(), WiFi.RSSI());
    } else {
        snprintf(buf, sizeof(buf),
            "{\"status\":\"ok\",\"mode\":\"sta\",\"ip\":\"0.0.0.0\","
            "\"ssid\":\"\",\"rssi\":0,\"connected\":false}");
    }
    apiServer.sendJson(200, buf);
}

void DashboardService::handleSystemInfo() {
    char buf[512];
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t freePsram = ESP.getPsramSize() - ESP.getFreePsram();
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
        0);

    apiServer.sendJson(200, buf);
}
