#include "StorageService.h"
#include "../logger/LoggerService.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <string.h>

extern LoggerService loggerService;

StorageService::StorageService() {}
StorageService::~StorageService() { deinit(); }

bool StorageService::init() {
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if (!partition) {
        loggerService.warning("Storage", "No SPIFFS partition found — continuing without filesystem");
        return true;
    }

    if (!LittleFS.begin(false, "/littlefs", 5, partition->label)) {
        delay(100);
        if (!LittleFS.begin(true, "/littlefs", 5, partition->label)) {
            loggerService.warning("Storage", "LittleFS mount failed — continuing without filesystem");
            return true;
        }
    }
    return true;
}

void StorageService::tick() {}

bool StorageService::deinit() {
    LittleFS.end();
    return true;
}

bool StorageService::isRunning() const {
    return true;
}

bool StorageService::begin() {
    return init();
}

bool StorageService::format() {
    return LittleFS.format();
}

size_t StorageService::getTotalSpace() const {
    return LittleFS.totalBytes();
}

size_t StorageService::getUsedSpace() const {
    return LittleFS.usedBytes();
}

bool StorageService::writeFile(const char* path, const uint8_t* data, size_t len) {
    File f = LittleFS.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.write(data, len);
    f.close();
    return written == len;
}

bool StorageService::readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& outLen) {
    File f = LittleFS.open(path, FILE_READ);
    if (!f) return false;
    outLen = f.read(buffer, maxLen);
    f.close();
    return true;
}

bool StorageService::deleteFile(const char* path) {
    return LittleFS.remove(path);
}

bool StorageService::fileExists(const char* path) const {
    return LittleFS.exists(path);
}

size_t StorageService::getFileSize(const char* path) const {
    File f = LittleFS.open(path, FILE_READ);
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
}

bool StorageService::listDir(const char* dir, char** outFiles, int maxCount, int& outCount) {
    File root = LittleFS.open(dir);
    if (!root) return false;
    File f = root.openNextFile();
    outCount = 0;
    while (f && outCount < maxCount) {
        strncpy(outFiles[outCount], f.name(), 255);
        outFiles[outCount][255] = '\0';
        outCount++;
        f = root.openNextFile();
    }
    return true;
}

bool StorageService::createDir(const char* path) {
    return LittleFS.mkdir(path);
}

bool StorageService::removeDir(const char* path) {
    return LittleFS.rmdir(path);
}
