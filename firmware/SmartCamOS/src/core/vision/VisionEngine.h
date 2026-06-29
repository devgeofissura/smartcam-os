#ifndef SMARTCAM_VISION_ENGINE_H
#define SMARTCAM_VISION_ENGINE_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamModule.h"
#include "../../sdk/interfaces/IFrameProcessor.h"
#include "../../sdk/interfaces/IDetector.h"

struct Blob {
    float x;
    float y;
    float width;
    float height;
    float area;
    uint8_t meanR;
    uint8_t meanG;
    uint8_t meanB;
};

struct ColorRange {
    uint8_t hMin;
    uint8_t hMax;
    uint8_t sMin;
    uint8_t sMax;
    uint8_t vMin;
    uint8_t vMax;
};

struct BlobResult {
    Blob blobs[32];
    int count;
};

class VisionEngine : public SmartCamModule, public IFrameProcessor {
public:
    bool begin() override;
    void update() override;
    bool stop() override;

    bool toGrayscale(Frame& frame);
    bool toHsv(Frame& frame);
    bool threshold(Frame& frame, uint8_t min, uint8_t max);
    bool colorFilter(Frame& frame, const ColorRange& range);
    int findBlobs(const Frame& frame, Blob* out, int maxCount);
    bool drawOverlay(Frame& frame, const Blob& blob);
    bool drawOverlay(Frame& frame, const Detection& det, uint16_t color);
    bool drawOverlay(Frame& frame, const Detection& det);

    // Working buffer access for processed frames
    uint8_t* getWorkingBuffer() const;
    int getWorkingWidth() const;
    int getWorkingHeight() const;

    bool process(Frame& frame) override { return true; }
    const char* processorName() const override { return "VisionEngine"; }
    const char* name() const override { return "VisionEngine"; }

private:
    uint8_t* m_workBuffer;
    int m_workWidth;
    int m_workHeight;
    size_t m_workSize;
    bool m_initialized;

    void hsvFromRgb(uint8_t r, uint8_t g, uint8_t b, uint8_t& h, uint8_t& s, uint8_t& v) const;
    bool isInColorRange(uint8_t h, uint8_t s, uint8_t v, const ColorRange& range) const;
    void floodFill(const uint8_t* binary, int width, int height, int sx, int sy,
                   bool* visited, Blob& blob) const;

public:
    VisionEngine();
    ~VisionEngine();
};

#endif
