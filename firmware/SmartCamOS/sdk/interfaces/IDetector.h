#ifndef IDETECTOR_H
#define IDETECTOR_H

#include <Arduino.h>

struct Detection {
    float x;
    float y;
    float width;
    float height;
    float confidence;
    int classId;
    char label[32];
};

class IDetector {
public:
    virtual ~IDetector() = default;
    virtual bool loadModel() = 0;
    virtual bool runInference(uint8_t* frame, int width, int height, size_t size = 0) = 0;
    virtual int getDetections(Detection* out, int maxCount) = 0;
    virtual void unloadModel() = 0;
};

#endif
