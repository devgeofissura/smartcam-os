#ifndef SMARTCAM_STORAGE_SERVICE_H
#define SMARTCAM_STORAGE_SERVICE_H

#include <Arduino.h>
#include "../../sdk/interfaces/SmartCamService.h"

class StorageService : public SmartCamService {
public:
    bool init() override;
    void tick() override;
    bool deinit() override;
    bool isRunning() const override;

    bool begin();
    bool format();
    size_t getTotalSpace() const;
    size_t getUsedSpace() const;

    bool writeFile(const char* path, const uint8_t* data, size_t len);
    bool readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& outLen);
    bool deleteFile(const char* path);
    bool fileExists(const char* path) const;
    size_t getFileSize(const char* path) const;

    bool listDir(const char* dir, char** outFiles, int maxCount, int& outCount);
    bool createDir(const char* path);
    bool removeDir(const char* path);

    const char* name() const override { return "StorageService"; }

    StorageService();
    ~StorageService();
};

#endif
