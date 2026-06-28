#ifndef SMARTCAM_CAMERA_ENGINE_H
#define SMARTCAM_CAMERA_ENGINE_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamModule.h"
#include "../../sdk/interfaces/IFrameProcessor.h"

struct CameraPins {
    int pwdn = -1;
    int reset = -1;
    int xclk = 15;
    int siod = 4;
    int sioc = 5;
    int d7 = 16;
    int d6 = 17;
    int d5 = 18;
    int d4 = 19;
    int d3 = 20;
    int d2 = 21;
    int d1 = 47;
    int d0 = 48;
    int vsync = 6;
    int href = 7;
    int pclk = 13;
};

struct CameraConfig {
    int xclkFreq = 20000000;
    int frameSize = 8;      // 0-13: 96x96 to 1600x1200
    int jpegQuality = 12;   // 10-63 (lower = better)
    int fbCount = 2;        // 1 or 2
    int gmacSize = 0;       // Extra GMAC buffer (bytes)
};

struct CameraFrame {
    uint8_t* data;
    size_t len;
    int width;
    int height;
    int format; // 0=JPEG, 1=RGB565, 2=GRAYSCALE
    unsigned long timestamp;
};

class CameraEngine : public SmartCamModule {
public:
    bool begin() override;
    void update() override;
    bool stop() override;

    bool startStream();
    bool stopStream();
    bool setConfig(const CameraConfig& config);
    CameraConfig getConfig() const;
    CameraPins getPins() const;
    bool setPins(const CameraPins& pins);
    bool captureFrame();
    const CameraFrame* getCurrentFrame() const;
    bool addProcessor(IFrameProcessor* processor);
    bool removeProcessor(IFrameProcessor* processor);
    int getFps() const;
    bool isStreaming() const;
    bool isInitialized() const;

    const char* name() const override { return "CameraEngine"; }

private:
    CameraConfig m_config;
    CameraPins m_pins;
    CameraFrame m_frame;
    bool m_initialized;
    bool m_streaming;
    int m_fps;
    unsigned long m_lastFrameTime;
    int m_frameCount;
    unsigned long m_fpsTimer;
    IFrameProcessor* m_processors[4];
    int m_processorCount;

    void resetFrame();
    void updateFps();

public:
    CameraEngine();
    ~CameraEngine();
};

#endif
