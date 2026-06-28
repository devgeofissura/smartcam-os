#include "StorageService.h"
#include <LittleFS.h>
#include <string.h>

StorageService::StorageService() {}
StorageService::~StorageService() { deinit(); }

bool StorageService::init() {
    if (!LittleFS.begin(false)) {
        if (!LittleFS.begin(true)) {
            return false;
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
