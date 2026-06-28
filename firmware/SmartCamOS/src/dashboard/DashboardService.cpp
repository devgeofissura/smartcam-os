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
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>GeoFissura</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<div id="app">
  <aside id="sidebar">
    <div class="sidebar-header">
      <div class="logo">&#9670;</div>
      <h2>GeoFissura</h2>
    </div>
    <nav>
      <a href="#" data-page="dashboard" class="active">
        <span class="nav-icon">&#9632;</span> Dashboard
      </a>
      <a href="#" data-page="camera">
        <span class="nav-icon">&#9678;</span> Camera
      </a>
      <a href="#" data-page="motion">
        <span class="nav-icon">&#9650;</span> Motion
      </a>
      <a href="#" data-page="tracking">
        <span class="nav-icon">&#9673;</span> Tracking
      </a>
      <a href="#" data-page="settings">
        <span class="nav-icon">&#9881;</span> Settings
      </a>
    </nav>
    <div class="sidebar-footer">
      <span id="connection-status" class="disconnected">&#9679; Offline</span>
    </div>
  </aside>
  <main id="content">
    <header class="page-header">
      <h1 id="page-title">Dashboard</h1>
    </header>
    <div id="page-dashboard">
      <div class="grid">
        <div class="card"><div class="card-header"><span class="card-icon">&#9881;</span><h3>System</h3></div><div id="sys-info" class="card-body"><p>Loading...</p></div></div>
        <div class="card"><div class="card-header"><span class="card-icon">&#9783;</span><h3>Memory</h3></div><div id="mem-info" class="card-body"><p>Loading...</p></div></div>
        <div class="card"><div class="card-header"><span class="card-icon">&#9730;</span><h3>Network</h3></div><div id="net-info" class="card-body"><p>Loading...</p></div></div>
        <div class="card"><div class="card-header"><span class="card-icon">&#128247;</span><h3>Camera</h3></div><div id="cam-info" class="card-body"><p>No camera data</p></div></div>
      </div>
    </div>
    <div id="page-camera" style="display:none">
      <div class="card">
        <div class="card-header"><span class="card-icon">&#128247;</span><h3>Live Stream</h3></div>
        <div class="card-body stream-container">
          <img id="cam-stream" src="" alt="Camera Stream">
        </div>
      </div>
    </div>
    <div id="page-motion" style="display:none">
      <div class="card">
        <div class="card-header"><span class="card-icon">&#9650;</span><h3>Axis Control</h3></div>
        <div class="card-body">
          <div class="info-grid">
            <div class="info-item"><span class="info-label">Position</span><span class="info-value" id="mot-pos">0</span></div>
            <div class="info-item"><span class="info-label">Speed</span><span class="info-value" id="mot-speed">0</span></div>
            <div class="info-item"><span class="info-label">Homed</span><span class="info-value" id="mot-homed">-</span></div>
            <div class="info-item"><span class="info-label">Moving</span><span class="info-value" id="mot-moving">-</span></div>
            <div class="info-item"><span class="info-label">Enabled</span><span class="info-value" id="mot-enabled">-</span></div>
          </div>
          <div class="btn-group">
            <button class="btn btn-primary" onclick="sendMotion('home',0)">Home</button>
            <button class="btn btn-secondary" onclick="sendMotion('move',0,100)">+100</button>
            <button class="btn btn-secondary" onclick="sendMotion('move',0,-100)">-100</button>
            <button class="btn btn-danger" onclick="sendMotion('stop',0)">Stop</button>
            <button class="btn btn-success" onclick="sendMotion('enable',0,1)">Enable</button>
            <button class="btn btn-warning" onclick="sendMotion('enable',0,0)">Disable</button>
          </div>
        </div>
      </div>
    </div>
    <div id="page-tracking" style="display:none">
      <div class="card">
        <div class="card-header"><span class="card-icon">&#9673;</span><h3>Object Tracking</h3></div>
        <div class="card-body">
          <div class="info-grid">
            <div class="info-item"><span class="info-label">Target</span><span class="info-value" id="track-target">person</span></div>
            <div class="info-item"><span class="info-label">Status</span><span class="info-value" id="track-status">idle</span></div>
            <div class="info-item"><span class="info-label">Correction</span><span class="info-value" id="track-correction">0.0000</span></div>
            <div class="info-item"><span class="info-label">Position</span><span class="info-value" id="track-pos">(0, 0)</span></div>
            <div class="info-item"><span class="info-label">Size</span><span class="info-value" id="track-size">0x0</span></div>
            <div class="info-item"><span class="info-label">Confidence</span><span class="info-value" id="track-conf">0.00</span></div>
          </div>
          <div class="btn-group">
            <button class="btn btn-primary" onclick="trackAction('track-person')">Track Person</button>
            <button class="btn btn-secondary" onclick="trackAction('track-color','red')">Track Red</button>
            <button class="btn btn-danger" onclick="trackAction('stop')">Stop</button>
          </div>
        </div>
      </div>
    </div>
    <div id="page-settings" style="display:none">
      <div class="card">
        <div class="card-header"><span class="card-icon">&#9730;</span><h3>WiFi Configuration</h3></div>
        <div class="card-body">
          <div class="wifi-status-bar">
            <span>Status:</span>
            <span id="wifi-status">Loading...</span>
            <button class="btn btn-sm" onclick="wifiStatus()">Refresh</button>
          </div>
          <div class="form-group">
            <label for="wifi-networks">Network</label>
            <select id="wifi-networks">
              <option value="">-- Select a network --</option>
            </select>
          </div>
          <div class="form-group">
            <label for="wifi-password">Password</label>
            <input type="password" id="wifi-password" placeholder="Leave blank for open networks">
          </div>
          <div class="btn-group">
            <button class="btn btn-primary" id="wifi-scan-btn" onclick="wifiScan()">Scan</button>
            <button class="btn btn-success" id="wifi-connect-btn" onclick="wifiConnect()">Save &amp; Reboot</button>
          </div>
          <div id="wifi-msg" class="msg"></div>
        </div>
      </div>
    </div>
  </main>
</div>
<script src="/app.js"></script>
</body>
</html>
)rawliteral";

static const char STYLE_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg: #282a36;
  --bg-alt: #1e1f29;
  --current: #44475a;
  --selection: #44475a;
  --fg: #f8f8f2;
  --comment: #6272a4;
  --cyan: #8be9fd;
  --green: #50fa7b;
  --orange: #ffb86c;
  --pink: #ff79c6;
  --purple: #bd93f9;
  --red: #ff5555;
  --yellow: #f1fa8c;
  --radius: 10px;
  --shadow: 0 4px 12px rgba(0,0,0,.3);
}
*{margin:0;padding:0;box-sizing:border-box}
html,body{height:100%}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,sans-serif;background:var(--bg);color:var(--fg);line-height:1.5;overflow-x:hidden}
#app{display:flex;min-height:100vh}
#sidebar{width:220px;background:var(--bg-alt);display:flex;flex-direction:column;flex-shrink:0;border-right:1px solid var(--current)}
.sidebar-header{padding:1.25rem;display:flex;align-items:center;gap:.75rem;border-bottom:1px solid var(--current)}
.sidebar-header .logo{font-size:1.5rem;color:var(--pink)}
.sidebar-header h2{font-size:1.1rem;font-weight:700;color:var(--purple);letter-spacing:.5px}
#sidebar nav{flex:1;padding:.5rem}
#sidebar nav a{display:flex;align-items:center;gap:.75rem;padding:.7rem .75rem;margin-bottom:.2rem;border-radius:var(--radius);color:var(--comment);text-decoration:none;font-size:.9rem;transition:all .2s}
#sidebar nav a:hover{background:var(--current);color:var(--fg)}
#sidebar nav a.active{background:var(--purple);color:var(--bg);font-weight:600}
.nav-icon{font-size:.85rem;width:1.2rem;text-align:center}
.sidebar-footer{padding:1rem;border-top:1px solid var(--current);font-size:.8rem}
#connection-status{display:flex;align-items:center;gap:.4rem}
#connection-status.connected{color:var(--green)}
#connection-status.disconnected{color:var(--red)}
#connection-status.connecting{color:var(--yellow)}
#content{flex:1;display:flex;flex-direction:column;overflow-y:auto}
.page-header{padding:1.25rem 1.5rem;border-bottom:1px solid var(--current);background:var(--bg-alt)}
.page-header h1{font-size:1.25rem;font-weight:600;color:var(--pink)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:1rem;padding:1.5rem}
.card{background:var(--bg-alt);border-radius:var(--radius);box-shadow:var(--shadow);overflow:hidden;border:1px solid var(--current)}
.card-header{display:flex;align-items:center;gap:.6rem;padding:1rem 1.25rem;border-bottom:1px solid var(--current);background:rgba(68,71,90,.2)}
.card-icon{font-size:1rem;color:var(--purple)}
.card-header h3{font-size:.95rem;font-weight:600;color:var(--cyan)}
.card-body{padding:1.25rem}
.card-body p{margin:.35rem 0;font-size:.88rem;color:var(--comment)}
.info-grid{display:grid;grid-template-columns:1fr 1fr;gap:.5rem 1rem;margin-bottom:1rem}
.info-item{display:flex;justify-content:space-between;align-items:center;padding:.4rem 0;border-bottom:1px solid rgba(68,71,90,.3)}
.info-label{font-size:.82rem;color:var(--comment)}
.info-value{font-size:.88rem;color:var(--fg);font-weight:600;font-variant-numeric:tabular-nums}
.btn-group{display:flex;flex-wrap:wrap;gap:.5rem;margin-top:.75rem}
.btn{padding:.5rem 1rem;border:none;border-radius:6px;font-size:.82rem;font-weight:600;cursor:pointer;transition:all .15s;color:var(--bg)}
.btn:active{transform:scale(.96)}
.btn-primary{background:var(--purple)}
.btn-primary:hover{background:#a57ef5}
.btn-secondary{background:var(--current);color:var(--fg)}
.btn-secondary:hover{background:#5a5d77}
.btn-success{background:var(--green);color:var(--bg)}
.btn-success:hover{background:#3ce46a}
.btn-danger{background:var(--red)}
.btn-danger:hover{background:#e64444}
.btn-warning{background:var(--yellow);color:var(--bg)}
.btn-warning:hover{background:#e6e47c}
.btn-sm{padding:.3rem .6rem;font-size:.75rem;background:var(--current);color:var(--fg)}
.btn-sm:hover{background:var(--comment)}
.stream-container{text-align:center}
#cam-stream{max-width:100%;border-radius:6px;border:1px solid var(--current)}
.form-group{margin-bottom:1rem}
.form-group label{display:block;font-size:.82rem;color:var(--comment);margin-bottom:.35rem}
.form-group select,.form-group input{width:100%;padding:.6rem .75rem;background:var(--bg);color:var(--fg);border:1px solid var(--current);border-radius:6px;font-size:.88rem;outline:none;transition:border .2s}
.form-group select:focus,.form-group input:focus{border-color:var(--purple)}
.form-group select option{background:var(--bg);color:var(--fg)}
.wifi-status-bar{display:flex;align-items:center;gap:.75rem;margin-bottom:1rem;padding:.5rem .75rem;background:var(--bg);border-radius:6px;font-size:.85rem}
.wifi-status-bar span:first-child{color:var(--comment)}
.msg{margin-top:.75rem;padding:.5rem;border-radius:6px;font-size:.82rem;color:var(--fg);background:var(--current);display:none}
.msg.show{display:block}
@media(max-width:768px){
#sidebar{width:56px;overflow:hidden}
.sidebar-header h2,#sidebar nav a span:not(.nav-icon),.sidebar-footer span{display:none}
#sidebar nav a{justify-content:center;padding:.7rem 0}
#sidebar nav a .nav-icon{font-size:1.1rem;width:auto}
.sidebar-header{padding:.75rem;justify-content:center}
.sidebar-header .logo{font-size:1.3rem}
.info-grid{grid-template-columns:1fr}
.grid{padding:1rem}
.page-header{padding:1rem}
}
)rawliteral";

static const char APP_JS[] PROGMEM = R"rawliteral(
const API = window.location.origin;
let ws = null;
const pageNames = {dashboard:'Dashboard',camera:'Camera',motion:'Motion',tracking:'Tracking',settings:'Settings'};

function msg(text,type) {
const el = document.getElementById('wifi-msg');
if (!el) return;
el.textContent = text;
el.className = 'msg show';
if (type==='error') el.style.color='var(--red)';
else el.style.color='var(--fg)';
}

async function loadSystemInfo() {
try {
const r = await fetch(API + '/system');
const d = await r.json();
document.getElementById('sys-info').innerHTML =
'<div class="info-grid"><div class="info-item"><span class="info-label">Uptime</span><span class="info-value">'+d.uptime+'s</span></div>'+
'<div class="info-item"><span class="info-label">CPU</span><span class="info-value">'+d.cpuFreq+' MHz</span></div>'+
'<div class="info-item"><span class="info-label">Sketch</span><span class="info-value">'+d.sketchSize+' B</span></div>'+
'<div class="info-item"><span class="info-label">Free Sketch</span><span class="info-value">'+d.freeSketch+' B</span></div></div>';
document.getElementById('mem-info').innerHTML =
'<div class="info-grid"><div class="info-item"><span class="info-label">Free Heap</span><span class="info-value">'+formatBytes(d.freeHeap)+'</span></div>'+
'<div class="info-item"><span class="info-label">Min Heap</span><span class="info-value">'+formatBytes(d.minHeap)+'</span></div>'+
'<div class="info-item"><span class="info-label">Free PSRAM</span><span class="info-value">'+formatBytes(d.freePsram)+'</span></div>'+
'<div class="info-item"><span class="info-label">Total PSRAM</span><span class="info-value">'+formatBytes(d.totalPsram)+'</span></div></div>';
} catch(e) {
document.getElementById('sys-info').innerHTML='<p>Error loading data</p>';
document.getElementById('mem-info').innerHTML='<p>Error loading data</p>';
}
}

function formatBytes(b) {
if (!b||b===0) return '0 B';
const units = ['B','KB','MB'];
let i = 0;
let v = b;
while (v>=1024 && i<units.length-1) { v/=1024; i++; }
return v.toFixed(1)+' '+units[i];
}

async function loadNetworkInfo() {
try {
const r = await fetch(API + '/network');
const d = await r.json();
document.getElementById('net-info').innerHTML =
'<div class="info-grid"><div class="info-item"><span class="info-label">IP</span><span class="info-value">'+d.ip+'</span></div>'+
'<div class="info-item"><span class="info-label">SSID</span><span class="info-value">'+d.ssid+'</span></div>'+
'<div class="info-item"><span class="info-label">Signal</span><span class="info-value">'+d.rssi+' dBm</span></div>'+
'<div class="info-item"><span class="info-label">MAC</span><span class="info-value">'+d.mac+'</span></div></div>';
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
'<div class="info-grid"><div class="info-item"><span class="info-label">Status</span><span class="info-value">'+d.status+'</span></div>'+
'<div class="info-item"><span class="info-label">FPS</span><span class="info-value">'+d.fps+'</span></div>'+
'<div class="info-item"><span class="info-label">Resolution</span><span class="info-value">'+d.resolution+'</span></div></div>'+
'<div class="btn-group"><a class="btn btn-primary" href="/camera/stream" target="_blank">Open Stream</a></div>';
} catch(e) {
document.getElementById('cam-info').innerHTML='<p>Camera not available</p>';
}
}

function navigate(page) {
document.querySelectorAll('#sidebar nav a').forEach(function(l){l.classList.remove('active');});
const link = document.querySelector('#sidebar nav a[data-page="'+page+'"]');
if (link) link.classList.add('active');
document.querySelectorAll('#content > div[id^="page-"]').forEach(function(d){d.style.display='none';});
const el = document.getElementById('page-'+page);
if (el) el.style.display='block';
document.getElementById('page-title').textContent = pageNames[page]||page;
if (page==='camera') {
const img = document.getElementById('cam-stream');
if (img) img.src = '/camera/stream?'+Date.now();
}
if (page==='settings') { wifiStatus(); wifiScan(); }
}
document.querySelectorAll('#sidebar nav a').forEach(function(a){
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

async function trackAction(action, label) {
let body = 'action='+action;
if (label) body += '&label='+label;
try {
const r = await fetch(API + '/tracking', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
const d = await r.json();
if (d.status==='ok') msg(action + ' started');
} catch(e) { console.error(e); }
}

async function wifiStatus() {
try {
const r = await fetch('/api/wifi/status');
const d = await r.json();
document.getElementById('wifi-status').textContent = d.mode + ' (' + d.ip + (d.ssid ? ', ' + d.ssid : '') + ')';
} catch(e) {}
}

async function wifiScan() {
const btn = document.getElementById('wifi-scan-btn');
if (!btn) return;
btn.textContent='Scanning...'; btn.disabled=true;
try {
const r = await fetch('/api/wifi/scan');
const d = await r.json();
const sel = document.getElementById('wifi-networks');
sel.innerHTML = '<option value="">-- Select network --</option>';
if (d.networks && d.networks.length>0) {
d.networks.forEach(function(n){
const opt = document.createElement('option');
opt.value = n.ssid;
opt.textContent = n.ssid + ' (' + (n.rssi<-70?'&#9785;':n.rssi<-50?'&#9786;':'&#9787;') + ' ' + n.rssi + ' dBm)' + (n.open ? ' [open]' : '');
sel.appendChild(opt);
});
msg(d.networks.length + ' networks found');
} else {
msg('No networks found','error');
}} catch(e) {
msg('Scan failed','error');
}
btn.textContent='Scan'; btn.disabled=false;
}

async function wifiConnect() {
const ssid = document.getElementById('wifi-networks').value;
const pw = document.getElementById('wifi-password').value;
if (!ssid) { msg('Select a network','error'); return; }
msg('Saving and rebooting...');
try {
const body = 'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pw);
await fetch('/api/wifi/config', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
} catch(e) { msg('Config saved, rebooting...'); }
setTimeout(function(){ window.location.reload(); }, 3000);
}

setInterval(function(){
fetch('/tracking').then(function(r){return r.json();}).then(function(d){
const s = document.getElementById('track-status');
const c = document.getElementById('track-correction');
const p = document.getElementById('track-pos');
const sz = document.getElementById('track-size');
const cf = document.getElementById('track-conf');
if (s) s.textContent = d.locked ? 'locked' : 'searching';
if (c) c.textContent = d.correction.toFixed(4);
if (p) p.textContent = '('+d.tx+', '+d.ty+')';
if (sz) sz.textContent = d.tw+'x'+d.th;
if (cf) cf.textContent = d.conf.toFixed(2);
}).catch(function(){});
},1000);

function updateStatus(connected) {
const el = document.getElementById('connection-status');
if (!el) return;
if (connected) { el.innerHTML='&#9679; Online'; el.className='connected'; }
else { el.innerHTML='&#9679; Offline'; el.className='disconnected'; }
}

function refresh() { loadSystemInfo(); loadNetworkInfo(); loadCameraInfo(); loadMotionInfo(); }
setInterval(refresh, 5000);
document.addEventListener('DOMContentLoaded', function(){ navigate('dashboard'); });
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
    char* buf = new char[len + 1];
    if (!buf) { apiServer.sendError(503, "Out of memory"); return; }
    strcpy_P(buf, INDEX_HTML);
    apiServer.sendResponse(200, "text/html", buf);
    delete[] buf;
}

void DashboardService::handleCss() {
    size_t len = strlen_P(STYLE_CSS);
    char* buf = new char[len + 1];
    if (!buf) { apiServer.sendError(503, "Out of memory"); return; }
    strcpy_P(buf, STYLE_CSS);
    apiServer.sendResponse(200, "text/css", buf);
    delete[] buf;
}

void DashboardService::handleJs() {
    size_t len = strlen_P(APP_JS);
    char* buf = new char[len + 1];
    if (!buf) { apiServer.sendError(503, "Out of memory"); return; }
    strcpy_P(buf, APP_JS);
    apiServer.sendResponse(200, "application/javascript", buf);
    delete[] buf;
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
    static char buf[2048];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "{\"status\":\"ok\",\"level\":%d,\"count\":%d,\"entries\":[",
        (int)loggerService.getLevel(), loggerService.getEntryCount());

    static LogEntry entries[32];
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
    static char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"ready\":%s,\"width\":%d,\"height\":%d}",
        visionEngine.getWorkingBuffer() ? "true" : "false",
        visionEngine.getWorkingWidth(),
        visionEngine.getWorkingHeight());
    apiServer.sendJson(200, buf);
}

void DashboardService::handleDetectionInfo() {
    static char buf[1024];
    static Detection detections[8];
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
    static char buf[1024];
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
