#include "CameraEngine.h"
#include <esp_camera.h>

static CameraEngine* s_instance = nullptr;

static camera_config_t buildEspConfig(const CameraPins& pins, const CameraConfig& config) {
    camera_config_t c;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer = LEDC_TIMER_0;
    c.pin_d0 = pins.d0;
    c.pin_d1 = pins.d1;
    c.pin_d2 = pins.d2;
    c.pin_d3 = pins.d3;
    c.pin_d4 = pins.d4;
    c.pin_d5 = pins.d5;
    c.pin_d6 = pins.d6;
    c.pin_d7 = pins.d7;
    c.pin_xclk = pins.xclk;
    c.pin_pclk = pins.pclk;
    c.pin_vsync = pins.vsync;
    c.pin_href = pins.href;
    c.pin_sccb_sda = pins.siod;
    c.pin_sccb_scl = pins.sioc;
    c.pin_pwdn = pins.pwdn;
    c.pin_reset = pins.reset;
    c.xclk_freq_hz = config.xclkFreq;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size = (framesize_t)config.frameSize;
    c.jpeg_quality = config.jpegQuality;
    c.fb_count = 1;
    c.grab_mode = CAMERA_GRAB_LATEST;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    return c;
}

CameraEngine::CameraEngine()
    : m_initialized(false)
    , m_streaming(false)
    , m_fps(0)
    , m_lastFrameTime(0)
    , m_frameCount(0)
    , m_fpsTimer(0)
    , m_processorCount(0)
    , m_currentFb(nullptr) {
    resetFrame();
}

CameraEngine::~CameraEngine() {
    stop();
}

void CameraEngine::resetFrame() {
    m_frame.data = nullptr;
    m_frame.size = 0;
    m_frame.width = 0;
    m_frame.height = 0;
    m_frame.bytesPerPixel = 0;
    m_frame.timestamp = 0;
}

bool CameraEngine::begin() {
    if (m_initialized) return true;

    camera_config_t espConfig = buildEspConfig(m_pins, m_config);

    esp_err_t err = esp_camera_init(&espConfig);
    if (err != ESP_OK) {
        m_initialized = false;
        return false;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, (framesize_t)m_config.frameSize);
        s->set_quality(s, m_config.jpegQuality);
        s->set_vflip(s, 1);
        s->set_hmirror(s, 1);
    }

    m_initialized = true;
    s_instance = this;
    return true;
}

void CameraEngine::update() {
    if (!m_streaming || !m_initialized) return;

    if (m_currentFb) {
        esp_camera_fb_return((camera_fb_t*)m_currentFb);
        m_currentFb = nullptr;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return;
    m_currentFb = fb;

    m_frame.data = fb->buf;
    m_frame.size = fb->len;
    m_frame.width = fb->width;
    m_frame.height = fb->height;
    m_frame.bytesPerPixel = (fb->format == PIXFORMAT_JPEG) ? 0 : (fb->format == PIXFORMAT_GRAYSCALE) ? 1 : 3;
    m_frame.timestamp = millis();

    for (int i = 0; i < m_processorCount; i++) {
        if (m_processors[i]) {
            m_processors[i]->process(m_frame);
        }
    }

    m_frameCount++;
    updateFps();
}

bool CameraEngine::stop() {
    m_streaming = false;
    if (m_currentFb) {
        esp_camera_fb_return((camera_fb_t*)m_currentFb);
        m_currentFb = nullptr;
    }
    if (m_initialized) {
        esp_camera_deinit();
        m_initialized = false;
    }
    resetFrame();
    s_instance = nullptr;
    return true;
}

bool CameraEngine::startStream() {
    if (!m_initialized) return false;
    m_streaming = true;
    m_frameCount = 0;
    m_fpsTimer = millis();
    return true;
}

bool CameraEngine::stopStream() {
    m_streaming = false;
    return true;
}

bool CameraEngine::setConfig(const CameraConfig& config) {
    if (m_initialized) return false;
    m_config = config;
    return true;
}

CameraConfig CameraEngine::getConfig() const {
    return m_config;
}

CameraPins CameraEngine::getPins() const {
    return m_pins;
}

bool CameraEngine::setPins(const CameraPins& pins) {
    if (m_initialized) return false;
    m_pins = pins;
    return true;
}

bool CameraEngine::captureFrame() {
    if (!m_initialized) return false;

    if (m_currentFb) {
        esp_camera_fb_return((camera_fb_t*)m_currentFb);
        m_currentFb = nullptr;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;
    m_currentFb = fb;

    m_frame.data = fb->buf;
    m_frame.size = fb->len;
    m_frame.width = fb->width;
    m_frame.height = fb->height;
    m_frame.bytesPerPixel = (fb->format == PIXFORMAT_JPEG) ? 0 : (fb->format == PIXFORMAT_GRAYSCALE) ? 1 : 3;
    m_frame.timestamp = millis();

    return true;
}

bool CameraEngine::getFrame(uint8_t** out, int* w, int* h) {
    if (!out || !w || !h) return false;
    *out = m_frame.data;
    *w = m_frame.width;
    *h = m_frame.height;
    return m_frame.data != nullptr;
}

void CameraEngine::returnFrame() {
}

const Frame* CameraEngine::getCurrentFrame() const {
    return &m_frame;
}

bool CameraEngine::addProcessor(IFrameProcessor* processor) {
    if (!processor || m_processorCount >= 4) return false;
    m_processors[m_processorCount++] = processor;
    return true;
}

bool CameraEngine::removeProcessor(IFrameProcessor* processor) {
    for (int i = 0; i < m_processorCount; i++) {
        if (m_processors[i] == processor) {
            for (int j = i; j < m_processorCount - 1; j++) {
                m_processors[j] = m_processors[j + 1];
            }
            m_processorCount--;
            return true;
        }
    }
    return false;
}

int CameraEngine::getFps() const {
    return m_fps;
}

bool CameraEngine::isStreaming() const {
    return m_streaming;
}

bool CameraEngine::isInitialized() const {
    return m_initialized;
}

void CameraEngine::updateFps() {
    unsigned long now = millis();
    if (now - m_fpsTimer >= 1000) {
        m_fps = m_frameCount;
        m_frameCount = 0;
        m_fpsTimer = now;
    }
}
