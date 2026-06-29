#ifndef SMARTCAM_API_SERVER_H
#define SMARTCAM_API_SERVER_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamService.h"

class ApiServer : public SmartCamService {
public:
    bool init() override;
    void tick() override;
    bool deinit() override;
    bool isRunning() const override;

    bool start(int port = 80);
    bool stop();
    bool registerEndpoint(const char* method, const char* path, void (*handler)());
    bool unregisterEndpoint(const char* method, const char* path);
    bool sendResponse(int code, const char* contentType, const char* body);
    bool sendData(int code, const char* contentType, const char* data, size_t len);
    bool sendJson(int code, const char* json);
    bool sendError(int code, const char* message);

    const char* name() const override { return "ApiServer"; }

    // System info API (called by dashboard)
    void registerSystemEndpoints();

    // Streaming support for MJPEG
    bool beginStream(const char* contentType);
    bool streamChunk(const uint8_t* data, size_t len);
    bool endStream();

    // HTTP request helpers
    String getArg(const char* name) const;
    bool hasArg(const char* name) const;
    bool streamClientConnected() const;

private:
    int m_port;
    bool m_running;
    void* m_server; // WebServer*
    bool m_streaming;

    bool serveStaticFile(const char* path, const uint8_t* data, size_t len, const char* mime);

public:
    ApiServer();
    ~ApiServer();
};

#endif
