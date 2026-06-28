#include "ApiServer.h"
#include <WebServer.h>
#include <WiFi.h>

static ApiServer* s_instance = nullptr;
static WebServer* s_webServer = nullptr;

ApiServer::ApiServer()
    : m_port(80)
    , m_running(false)
    , m_server(nullptr)
    , m_streaming(false) {}

ApiServer::~ApiServer() {
    deinit();
}

bool ApiServer::init() {
    if (m_running) return true;
    m_running = true;
    s_instance = this;
    return true;
}

void ApiServer::tick() {
    if (s_webServer) {
        s_webServer->handleClient();
    }
}

bool ApiServer::deinit() {
    if (!m_running) return true;
    stop();
    m_running = false;
    s_instance = nullptr;
    return true;
}

bool ApiServer::isRunning() const {
    return m_running;
}

bool ApiServer::start(int port) {
    if (s_webServer) return true;

    m_port = port;
    s_webServer = new WebServer(port);
    if (!s_webServer) return false;

    s_webServer->begin();
    return true;
}

bool ApiServer::stop() {
    if (s_webServer) {
        s_webServer->stop();
        delete s_webServer;
        s_webServer = nullptr;
    }
    return true;
}

bool ApiServer::registerEndpoint(const char* method, const char* path, void (*handler)()) {
    if (!s_webServer || !path || !handler) return false;

    String m = String(method);
    m.toUpperCase();

    if (m == "GET") {
        s_webServer->on(path, HTTP_GET, handler);
    } else if (m == "POST") {
        s_webServer->on(path, HTTP_POST, handler);
    } else if (m == "PUT") {
        s_webServer->on(path, HTTP_PUT, handler);
    } else if (m == "DELETE") {
        s_webServer->on(path, HTTP_DELETE, handler);
    } else {
        return false;
    }
    return true;
}

bool ApiServer::unregisterEndpoint(const char* method, const char* path) {
    if (!s_webServer || !path) return false;
    s_webServer->on(path, HTTP_GET, nullptr);
    return true;
}

bool ApiServer::sendResponse(int code, const char* contentType, const char* body) {
    if (!s_webServer) return false;
    s_webServer->send(code, contentType, body);
    return true;
}

bool ApiServer::sendJson(int code, const char* json) {
    return sendResponse(code, "application/json", json);
}

bool ApiServer::sendError(int code, const char* message) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"error\":true,\"code\":%d,\"message\":\"%s\"}", code, message);
    return sendJson(code, buf);
}

bool ApiServer::serveStaticFile(const char* path, const uint8_t* data, size_t len, const char* mime) {
    if (!s_webServer) return false;
    s_webServer->on(path, HTTP_GET, [this, data, len, mime]() {
        String body((const char*)data, len);
        if (mime) {
            s_webServer->send(200, mime, body);
        } else {
            s_webServer->send(200, "text/html", body);
        }
    });
    return true;
}

void ApiServer::registerSystemEndpoints() {
    if (!s_webServer) return;
}

bool ApiServer::beginStream(const char* contentType) {
    if (!s_webServer || m_streaming) return false;
    m_streaming = true;
    s_webServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_webServer->send(200, contentType, "");
    return true;
}

bool ApiServer::streamChunk(const uint8_t* data, size_t len) {
    if (!s_webServer || !m_streaming || !data || len == 0) return false;
    s_webServer->sendContent_P((const char*)data, len);
    return true;
}

bool ApiServer::endStream() {
    if (!s_webServer || !m_streaming) return false;
    s_webServer->sendContent("");
    m_streaming = false;
    return true;
}

String ApiServer::getArg(const char* name) const {
    if (!s_webServer || !name) return String();
    return s_webServer->arg(name);
}

bool ApiServer::hasArg(const char* name) const {
    if (!s_webServer || !name) return false;
    return s_webServer->hasArg(name);
}
