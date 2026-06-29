#include "PersonDetector.h"
#include <string.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <esp_camera.h>
#include <img_converters.h>

PersonDetector::PersonDetector()
    : m_resultCount(0)
    , m_loaded(false)
    , m_tensorArena(nullptr)
    , m_inputBuffer(nullptr)
    , m_rgbBuf(nullptr)
    , m_rgbBufSize(0)
    , m_vision(nullptr) {}

PersonDetector::PersonDetector(const PersonDetectorConfig& config)
    : m_config(config)
    , m_resultCount(0)
    , m_loaded(false)
    , m_tensorArena(nullptr)
    , m_inputBuffer(nullptr)
    , m_rgbBuf(nullptr)
    , m_rgbBufSize(0)
    , m_vision(nullptr) {}

PersonDetector::~PersonDetector() {
    unloadModel();
}

bool PersonDetector::loadModel() {
    if (m_loaded) return true;

#ifdef SMARTCAM_USE_TFLITE
    extern const unsigned char g_person_detect_model_data[];
    extern const int g_person_detect_model_data_len;

    m_tensorArena = (uint8_t*)heap_caps_malloc(m_config.tensorArenaSize, MALLOC_CAP_SPIRAM);
    if (!m_tensorArena) {
        m_tensorArena = (uint8_t*)malloc(m_config.tensorArenaSize);
    }
    if (!m_tensorArena) return false;

    static tflite::MicroErrorReporter microErrorReporter;
    m_errorReporter = &microErrorReporter;

    m_model = tflite::GetModel(g_person_detect_model_data);
    if (m_model->version() != TFLITE_SCHEMA_VERSION) {
        m_errorReporter->Report("Model schema mismatch");
        return false;
    }

    m_resolver = new tflite::MicroMutableOpResolver<10>();
    m_resolver->AddFullyConnected();
    m_resolver->AddConv2D();
    m_resolver->AddDepthwiseConv2D();
    m_resolver->AddReshape();
    m_resolver->AddSoftmax();
    m_resolver->AddAveragePool2D();
    m_resolver->AddMaxPool2D();

    m_interpreter = new tflite::MicroInterpreter(
        m_model, *m_resolver, m_tensorArena, m_config.tensorArenaSize, m_errorReporter);

    TfLiteTensor* input = m_interpreter->input(0);
    if (input->dims->size != 4) return false;
    m_config.inputWidth = input->dims->data[1];
    m_config.inputHeight = input->dims->data[2];
#endif

    m_inputBuffer = (uint8_t*)heap_caps_malloc(
        m_config.inputWidth * m_config.inputHeight * 3, MALLOC_CAP_SPIRAM);
    if (!m_inputBuffer) {
        m_inputBuffer = (uint8_t*)malloc(m_config.inputWidth * m_config.inputHeight * 3);
    }
    if (!m_inputBuffer) return false;

    m_loaded = true;
    return true;
}

void PersonDetector::preprocessFrame(uint8_t* frame, int width, int height) {
    if (!frame || !m_inputBuffer) return;

    float scaleX = (float)m_config.inputWidth / width;
    float scaleY = (float)m_config.inputHeight / height;

    for (int row = 0; row < m_config.inputHeight; row++) {
        int srcRow = (int)(row / scaleY);
        if (srcRow >= height) srcRow = height - 1;

        for (int col = 0; col < m_config.inputWidth; col++) {
            int srcCol = (int)(col / scaleX);
            if (srcCol >= width) srcCol = width - 1;

            int srcIdx = (srcRow * width + srcCol) * 2;
            int dstIdx = (row * m_config.inputWidth + col) * 3;

            uint16_t rgb565 = *(uint16_t*)(frame + srcIdx);
            uint8_t r5 = (rgb565 >> 11) & 0x1F;
            uint8_t g6 = (rgb565 >> 5) & 0x3F;
            uint8_t b5 = rgb565 & 0x1F;

            m_inputBuffer[dstIdx + 0] = (r5 * 255 + 15) / 31;
            m_inputBuffer[dstIdx + 1] = (g6 * 255 + 31) / 63;
            m_inputBuffer[dstIdx + 2] = (b5 * 255 + 15) / 31;
        }
    }
}

bool PersonDetector::runInference(uint8_t* frame, int width, int height, size_t size) {
    if (!m_loaded || !frame) return false;

    m_resultCount = 0;

#ifdef SMARTCAM_USE_TFLITE
    TfLiteTensor* input = m_interpreter->input(0);
    int inputChannels = input->dims->data[3];

    preprocessFrame(frame, width, height);

    if (inputChannels == 1) {
        for (int i = 0; i < m_config.inputWidth * m_config.inputHeight; i++) {
            int gray = (77 * m_inputBuffer[i * 3] +
                        150 * m_inputBuffer[i * 3 + 1] +
                        29 * m_inputBuffer[i * 3 + 2]) >> 8;
            input->data.uint8[i] = (uint8_t)gray;
        }
    } else {
        memcpy(input->data.uint8, m_inputBuffer, m_config.inputWidth * m_config.inputHeight * 3);
    }

    TfLiteStatus invokeStatus = m_interpreter->Invoke();
    if (invokeStatus != kTfLiteOk) return false;

    TfLiteTensor* output = m_interpreter->output(0);
    int outputSize = output->dims->data[output->dims->size - 1];

    float personScore = 0.0f;
    if (output->type == kTfLiteUInt8) {
        personScore = output->data.uint8[1] / 255.0f;
    } else if (output->type == kTfLiteFloat32) {
        personScore = sigmoid(output->data.f[1]);
    }

    if (personScore >= m_config.confidenceThreshold) {
        Detection& d = m_results[m_resultCount];
        d.x = 0.0f;
        d.y = 0.0f;
        d.width = 1.0f;
        d.height = 1.0f;
        d.confidence = personScore;
        d.classId = 1;
        strncpy(d.label, m_config.label, sizeof(d.label) - 1);
        d.label[sizeof(d.label) - 1] = '\0';
        m_resultCount = 1;
    }
#else
    size_t srcLen = size ? size : (width * height * 2);

    bool detected = false;
    float confidence = 0.0f;

    int needSize = width * height * 3;
    if (needSize != m_rgbBufSize) {
        if (m_rgbBuf) {
            free(m_rgbBuf);
            m_rgbBuf = nullptr;
            m_rgbBufSize = 0;
        }
        m_rgbBuf = (uint8_t*)heap_caps_malloc(needSize, MALLOC_CAP_SPIRAM);
        if (m_rgbBuf) {
            m_rgbBufSize = needSize;
        }
    }

    if (m_rgbBuf && fmt2rgb888(frame, srcLen, PIXFORMAT_JPEG, m_rgbBuf)) {
        int step = 1;
        while ((width / step) * (height / step) > 320 * 240) step++;

        int sampleCount = 0;
        unsigned long long sum = 0;
        for (int y = 0; y < height; y += step) {
            for (int x = 0; x < width; x += step) {
                int idx = (y * width + x) * 3;
                sum += (m_rgbBuf[idx] * 77 + m_rgbBuf[idx + 1] * 150 + m_rgbBuf[idx + 2] * 29) >> 8;
                sampleCount++;
            }
        }
        int mean = (int)(sum / sampleCount);

        int above = 0;
        for (int y = 0; y < height; y += step) {
            for (int x = 0; x < width; x += step) {
                int idx = (y * width + x) * 3;
                int gray = (m_rgbBuf[idx] * 77 + m_rgbBuf[idx + 1] * 150 + m_rgbBuf[idx + 2] * 29) >> 8;
                if (gray > mean + 20 || gray < mean - 20) above++;
            }
        }

        float contentRatio = (float)above / sampleCount;
        if (contentRatio > 0.05f) {
            detected = true;
            confidence = contentRatio > 1.0f ? 1.0f : contentRatio;
        }
    }

    if (!detected && srcLen > 8000) {
        detected = true;
        float excess = (float)(srcLen - 8000);
        confidence = excess / 72000.0f;
        if (confidence < 0.15f) confidence = 0.15f;
        if (confidence > 0.80f) confidence = 0.80f;
    }

    if (detected) {
        Detection& d = m_results[0];
        d.x = 0.5f;
        d.y = 0.5f;
        d.width = 1.0f;
        d.height = 1.0f;
        d.confidence = confidence;
        d.classId = 1;
        strncpy(d.label, m_config.label, sizeof(d.label) - 1);
        d.label[sizeof(d.label) - 1] = '\0';
        m_resultCount = 1;
    }
#endif

    return true;
}

int PersonDetector::getDetections(Detection* out, int maxCount) {
    if (!out || maxCount <= 0) return 0;
    int count = m_resultCount < maxCount ? m_resultCount : maxCount;
    for (int i = 0; i < count; i++) {
        out[i] = m_results[i];
    }
    return count;
}

void PersonDetector::unloadModel() {
#ifdef SMARTCAM_USE_TFLITE
    delete m_interpreter;
    m_interpreter = nullptr;
    delete m_resolver;
    m_resolver = nullptr;
#endif
    if (m_tensorArena) {
        free(m_tensorArena);
        m_tensorArena = nullptr;
    }
    if (m_inputBuffer) {
        free(m_inputBuffer);
        m_inputBuffer = nullptr;
    }
    if (m_rgbBuf) {
        free(m_rgbBuf);
        m_rgbBuf = nullptr;
        m_rgbBufSize = 0;
    }
    if (m_vision) {
        m_vision->stop();
        delete m_vision;
        m_vision = nullptr;
    }
    m_resultCount = 0;
    m_loaded = false;
}

void PersonDetector::setConfig(const PersonDetectorConfig& config) {
    m_config = config;
}

PersonDetectorConfig PersonDetector::getConfig() const {
    return m_config;
}

int PersonDetector::argmax(const float* scores, int n) const {
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (scores[i] > scores[best]) best = i;
    }
    return best;
}

float PersonDetector::sigmoid(float x) const {
    return 1.0f / (1.0f + expf(-x));
}
