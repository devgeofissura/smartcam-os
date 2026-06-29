#include "VisionEngine.h"
#include <esp_heap_caps.h>
#include <string.h>

static VisionEngine* s_instance = nullptr;

static const int MAX_WORK_SIZE = 320 * 240; // QVGA

VisionEngine::VisionEngine()
    : m_workBuffer(nullptr)
    , m_workWidth(0)
    , m_workHeight(0)
    , m_workSize(0)
    , m_initialized(false) {}

VisionEngine::~VisionEngine() {
    stop();
}

bool VisionEngine::begin() {
    if (m_initialized) return true;

    m_workSize = MAX_WORK_SIZE;
    m_workBuffer = (uint8_t*)heap_caps_malloc(m_workSize, MALLOC_CAP_SPIRAM);
    if (!m_workBuffer) {
        m_workBuffer = (uint8_t*)malloc(m_workSize);
    }
    if (!m_workBuffer) return false;

    memset(m_workBuffer, 0, m_workSize);
    m_initialized = true;
    s_instance = this;
    return true;
}

void VisionEngine::update() {}

bool VisionEngine::stop() {
    if (m_workBuffer) {
        free(m_workBuffer);
        m_workBuffer = nullptr;
    }
    m_initialized = false;
    s_instance = nullptr;
    return true;
}

bool VisionEngine::toGrayscale(Frame& frame) {
    if (!m_initialized || !frame.data || frame.bytesPerPixel < 2) return false;

    int pixels = frame.width * frame.height;
    size_t needed = pixels;
    if (needed > m_workSize) return false;

    uint16_t* src = (uint16_t*)frame.data;
    for (int i = 0; i < pixels; i++) {
        uint16_t pixel = src[i];
        uint8_t r = (pixel >> 11) & 0x1F;
        uint8_t g = (pixel >> 5) & 0x3F;
        uint8_t b = pixel & 0x1F;
        r = (r * 255) / 31;
        g = (g * 255) / 63;
        b = (b * 255) / 31;
        m_workBuffer[i] = (r * 77 + g * 150 + b * 29) >> 8;
    }

    m_workWidth = frame.width;
    m_workHeight = frame.height;
    return true;
}

void VisionEngine::hsvFromRgb(uint8_t r, uint8_t g, uint8_t b,
                               uint8_t& h, uint8_t& s, uint8_t& v) const {
    uint8_t min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    uint8_t max = r > g ? (r > b ? r : b) : (g > b ? g : b);

    v = max;
    if (max == 0) {
        h = 0;
        s = 0;
        return;
    }

    int diff = max - min;
    s = (diff * 255) / max;

    if (diff == 0) {
        h = 0;
        return;
    }

    int hCalc;
    if (max == r) {
        hCalc = ((int)(g - b) * 60) / diff;
    } else if (max == g) {
        hCalc = ((int)(b - r) * 60) / diff + 120;
    } else {
        hCalc = ((int)(r - g) * 60) / diff + 240;
    }

    if (hCalc < 0) hCalc += 360;
    h = (hCalc * 255) / 360;
}

bool VisionEngine::toHsv(Frame& frame) {
    if (!m_initialized || !frame.data || frame.bytesPerPixel < 2) return false;

    int pixels = frame.width * frame.height;
    size_t needed = pixels * 3;
    if (needed > m_workSize) return false;

    uint16_t* src = (uint16_t*)frame.data;
    uint8_t* dst = m_workBuffer;

    for (int i = 0; i < pixels; i++) {
        uint16_t pixel = src[i];
        uint8_t r5 = (pixel >> 11) & 0x1F;
        uint8_t g6 = (pixel >> 5) & 0x3F;
        uint8_t b5 = pixel & 0x1F;
        uint8_t r = (r5 * 255) / 31;
        uint8_t g = (g6 * 255) / 63;
        uint8_t b = (b5 * 255) / 31;

        uint8_t h, s, v;
        hsvFromRgb(r, g, b, h, s, v);
        *dst++ = h;
        *dst++ = s;
        *dst++ = v;
    }

    m_workWidth = frame.width;
    m_workHeight = frame.height;
    return true;
}

bool VisionEngine::threshold(Frame& frame, uint8_t min, uint8_t max) {
    if (!m_initialized || !frame.data) return false;
    if (frame.bytesPerPixel >= 2) {
        if (!toGrayscale(frame)) return false;
    }

    int pixels = m_workWidth * m_workHeight;
    if ((size_t)pixels > m_workSize) return false;

    for (int i = 0; i < pixels; i++) {
        m_workBuffer[i] = (m_workBuffer[i] >= min && m_workBuffer[i] <= max) ? 255 : 0;
    }
    return true;
}

bool VisionEngine::colorFilter(Frame& frame, const ColorRange& range) {
    if (!m_initialized || !frame.data) return false;
    if (!toHsv(frame)) return false;

    int pixels = m_workWidth * m_workHeight;
    uint8_t* hsv = m_workBuffer;

    for (int i = 0; i < pixels; i++) {
        uint8_t h = hsv[i * 3];
        uint8_t s = hsv[i * 3 + 1];
        uint8_t v = hsv[i * 3 + 2];

        bool match = isInColorRange(h, s, v, range);
        m_workBuffer[i] = match ? 255 : 0;
    }
    return true;
}

bool VisionEngine::isInColorRange(uint8_t h, uint8_t s, uint8_t v,
                                   const ColorRange& range) const {
    if (s < range.sMin || s > range.sMax) return false;
    if (v < range.vMin || v > range.vMax) return false;

    if (range.hMin <= range.hMax) {
        return h >= range.hMin && h <= range.hMax;
    } else {
        return h >= range.hMin || h <= range.hMax;
    }
}

void VisionEngine::floodFill(const uint8_t* binary, int width, int height,
                              int sx, int sy, bool* visited, Blob& blob) const {
    int stack[4096];
    int stackPos = 0;
    long sumX = 0, sumY = 0;
    int minX = sx, maxX = sx, minY = sy, maxY = sy;
    int count = 0;
    uint32_t sumR = 0, sumG = 0, sumB = 0;

    stack[stackPos++] = sx + sy * width;

    while (stackPos > 0) {
        int pos = stack[--stackPos];
        int x = pos % width;
        int y = pos / width;

        if (x < 0 || x >= width || y < 0 || y >= height) continue;
        if (visited[pos]) continue;
        if (binary[pos] == 0) continue;

        visited[pos] = true;
        count++;
        sumX += x;
        sumY += y;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;

        if (stackPos + 4 < 4096) {
            stack[stackPos++] = (x - 1) + y * width;
            stack[stackPos++] = (x + 1) + y * width;
            stack[stackPos++] = x + (y - 1) * width;
            stack[stackPos++] = x + (y + 1) * width;
        }
    }

    if (count > 20) {
        blob.x = (float)sumX / count;
        blob.y = (float)sumY / count;
        blob.width = (float)(maxX - minX + 1);
        blob.height = (float)(maxY - minY + 1);
        blob.area = (float)count;
        blob.meanR = (uint8_t)(sumR / count);
        blob.meanG = (uint8_t)(sumG / count);
        blob.meanB = (uint8_t)(sumB / count);
    } else {
        blob.area = 0;
    }
}

int VisionEngine::findBlobs(const Frame& frame, Blob* out, int maxCount) {
    if (!m_initialized || !out || maxCount <= 0) return 0;

    int pixels = m_workWidth * m_workHeight;
    if (pixels <= 0) return 0;

    bool* visited = (bool*)calloc(pixels, sizeof(bool));
    if (!visited) return 0;

    int blobCount = 0;

    for (int y = 0; y < m_workHeight && blobCount < maxCount; y++) {
        for (int x = 0; x < m_workWidth && blobCount < maxCount; x++) {
            int pos = x + y * m_workWidth;
            if (!visited[pos] && m_workBuffer[pos] > 0) {
                Blob blob;
                floodFill(m_workBuffer, m_workWidth, m_workHeight, x, y, visited, blob);
                if (blob.area > 0) {
                    out[blobCount++] = blob;
                }
            }
        }
    }

    free(visited);
    return blobCount;
}

bool VisionEngine::drawOverlay(Frame& frame, const Blob& blob) {
    if (!m_initialized || !frame.data || frame.bytesPerPixel < 2) return false;

    int width = frame.width;
    int height = frame.height;
    uint16_t* pixels = (uint16_t*)frame.data;

    int x1 = (int)(blob.x - blob.width / 2);
    int y1 = (int)(blob.y - blob.height / 2);
    int x2 = (int)(blob.x + blob.width / 2);
    int y2 = (int)(blob.y + blob.height / 2);

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= width) x2 = width - 1;
    if (y2 >= height) y2 = height - 1;

    uint16_t color = 0xF800;

    for (int x = x1; x <= x2; x++) {
        pixels[y1 * width + x] = color;
        pixels[y2 * width + x] = color;
    }
    for (int y = y1; y <= y2; y++) {
        pixels[y * width + x1] = color;
        pixels[y * width + x2] = color;
    }

    return true;
}

bool VisionEngine::drawOverlay(Frame& frame, const Detection& det, uint16_t color) {
    if (!frame.data || frame.bytesPerPixel < 2) return false;

    int width = frame.width;
    int height = frame.height;
    uint16_t* pixels = (uint16_t*)frame.data;

    int x1 = (int)((det.x - det.width / 2.0f) * width);
    int y1 = (int)((det.y - det.height / 2.0f) * height);
    int x2 = (int)((det.x + det.width / 2.0f) * width);
    int y2 = (int)((det.y + det.height / 2.0f) * height);

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= width) x2 = width - 1;
    if (y2 >= height) y2 = height - 1;

    for (int x = x1; x <= x2; x++) {
        pixels[y1 * width + x] = color;
        pixels[y2 * width + x] = color;
    }
    for (int y = y1; y <= y2; y++) {
        pixels[y * width + x1] = color;
        pixels[y * width + x2] = color;
    }

    return true;
}

bool VisionEngine::drawOverlay(Frame& frame, const Detection& det) {
    return drawOverlay(frame, det, 0x07E0);
}

uint8_t* VisionEngine::getWorkingBuffer() const {
    return m_workBuffer;
}

int VisionEngine::getWorkingWidth() const {
    return m_workWidth;
}

int VisionEngine::getWorkingHeight() const {
    return m_workHeight;
}
