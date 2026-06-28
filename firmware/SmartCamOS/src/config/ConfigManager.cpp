#include "ConfigManager.h"
#include "../logger/LoggerService.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_partition.h>

extern LoggerService loggerService;

static ConfigManager* s_instance = nullptr;

static const char* CONFIG_FILE = "/config/system.json";
static const char* PROFILES_DIR = "/config/profiles/";

ConfigManager::ConfigManager()
    : m_schemaCount(0)
    , m_running(false)
    , m_dirty(false) {
    m_currentProfile[0] = '\0';
}

ConfigManager::~ConfigManager() {
    deinit();
}

bool ConfigManager::init() {
    if (m_running) return true;
    m_running = true;
    s_instance = this;

    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if (partition && LittleFS.begin(false, "/littlefs", 5, partition->label)) {
        if (!LittleFS.exists("/config")) {
            LittleFS.mkdir("/config");
        }
        if (!LittleFS.exists(PROFILES_DIR)) {
            LittleFS.mkdir(PROFILES_DIR);
        }
        load();
    } else {
        loggerService.warning("Config", "No filesystem — running without persistent config");
    }
    return true;
}

void ConfigManager::tick() {
    if (m_dirty) {
        save();
        m_dirty = false;
    }
}

bool ConfigManager::deinit() {
    if (!m_running) return true;
    if (m_dirty) save();
    m_running = false;
    s_instance = nullptr;
    return true;
}

bool ConfigManager::isRunning() const {
    return m_running;
}

bool ConfigManager::registerSchema(const ConfigSchema* schema, int count) {
    if (!schema || count <= 0 || m_schemaCount + count > MAX_SCHEMA_ENTRIES) return false;
    for (int i = 0; i < count; i++) {
        m_schemas[m_schemaCount++] = schema[i];
    }
    return true;
}

bool ConfigManager::validateKey(const char* key) const {
    if (!key) return false;
    for (int i = 0; i < m_schemaCount; i++) {
        if (strcmp(m_schemas[i].key, key) == 0) return true;
    }
    return false;
}

int ConfigManager::findSchema(const char* key) const {
    for (int i = 0; i < m_schemaCount; i++) {
        if (strcmp(m_schemas[i].key, key) == 0) return i;
    }
    return -1;
}

bool ConfigManager::setValue(const char* key, const char* value) {
    if (!m_running || !validateKey(key) || !value) return false;

    File file = LittleFS.open(CONFIG_FILE, "r");
    StaticJsonDocument<2048> doc;
    if (file) {
        DeserializationError err = deserializeJson(doc, file);
        file.close();
        if (err) doc.clear();
    }

    doc[key] = value;

    File outFile = LittleFS.open(CONFIG_FILE, "w");
    if (!outFile) return false;
    serializeJson(doc, outFile);
    outFile.close();
    return true;
}

bool ConfigManager::getValue(const char* key, char* out, size_t maxLen) const {
    if (!m_running || !validateKey(key) || !out || maxLen == 0) return false;

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) return false;

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) return false;

    const char* val = doc[key];
    if (!val) {
        int idx = findSchema(key);
        if (idx >= 0) {
            strncpy(out, m_schemas[idx].defaultValue, maxLen - 1);
            out[maxLen - 1] = '\0';
            return true;
        }
        return false;
    }

    strncpy(out, val, maxLen - 1);
    out[maxLen - 1] = '\0';
    return true;
}

bool ConfigManager::getInt(const char* key, int& out) const {
    char buf[64];
    if (!getValue(key, buf, sizeof(buf))) return false;
    out = atoi(buf);
    return true;
}

bool ConfigManager::getFloat(const char* key, float& out) const {
    char buf[64];
    if (!getValue(key, buf, sizeof(buf))) return false;
    out = atof(buf);
    return true;
}

bool ConfigManager::getBool(const char* key, bool& out) const {
    char buf[16];
    if (!getValue(key, buf, sizeof(buf))) return false;
    out = (strcmp(buf, "true") == 0 || strcmp(buf, "1") == 0 || strcmp(buf, "yes") == 0);
    return true;
}

bool ConfigManager::load() {
    if (!m_running) return false;

    if (!LittleFS.exists(CONFIG_FILE)) {
        resetToDefaults();
        return true;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) return false;

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        resetToDefaults();
        return false;
    }

    return true;
}

bool ConfigManager::save() {
    if (!m_running) return false;

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) return true;

    StaticJsonDocument<2048> doc;
    deserializeJson(doc, file);
    file.close();

    File outFile = LittleFS.open(CONFIG_FILE, "w");
    if (!outFile) return false;
    serializeJson(doc, outFile);
    outFile.close();
    return true;
}

bool ConfigManager::resetToDefaults() {
    if (!m_running) return false;

    StaticJsonDocument<2048> doc;
    for (int i = 0; i < m_schemaCount; i++) {
        doc[m_schemas[i].key] = m_schemas[i].defaultValue;
    }

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) return false;
    serializeJson(doc, file);
    file.close();
    return true;
}

bool ConfigManager::resetKey(const char* key) {
    if (!m_running || !validateKey(key)) return false;
    int idx = findSchema(key);
    if (idx < 0) return false;
    return setValue(key, m_schemas[idx].defaultValue);
}

bool ConfigManager::exportJson(char* buffer, size_t maxLen) const {
    if (!m_running || !buffer || maxLen == 0) return false;

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) return false;

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) return false;

    serializeJson(doc, buffer, maxLen);
    return true;
}

bool ConfigManager::importJson(const char* json) {
    if (!m_running || !json) return false;

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file) return false;
    serializeJson(doc, file);
    file.close();
    return true;
}

bool ConfigManager::setProfile(const char* profileName) {
    if (!m_running || !profileName) return false;
    strncpy(m_currentProfile, profileName, PROFILE_NAME_LEN - 1);
    m_currentProfile[PROFILE_NAME_LEN - 1] = '\0';
    return true;
}

bool ConfigManager::getProfile(char* out, size_t maxLen) const {
    if (!m_running || !out || maxLen == 0) return false;
    strncpy(out, m_currentProfile, maxLen - 1);
    out[maxLen - 1] = '\0';
    return true;
}

bool ConfigManager::saveProfile(const char* name) {
    if (!m_running || !name) return false;

    char path[64];
    snprintf(path, sizeof(path), "%s%s.json", PROFILES_DIR, name);

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) return false;

    StaticJsonDocument<2048> doc;
    deserializeJson(doc, file);
    file.close();

    File profileFile = LittleFS.open(path, "w");
    if (!profileFile) return false;
    serializeJson(doc, profileFile);
    profileFile.close();
    return true;
}

bool ConfigManager::loadProfile(const char* name) {
    if (!m_running || !name) return false;

    char path[64];
    snprintf(path, sizeof(path), "%s%s.json", PROFILES_DIR, name);

    if (!LittleFS.exists(path)) return false;

    File file = LittleFS.open(path, "r");
    if (!file) return false;

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) return false;

    File outFile = LittleFS.open(CONFIG_FILE, "w");
    if (!outFile) return false;
    serializeJson(doc, outFile);
    outFile.close();

    strncpy(m_currentProfile, name, PROFILE_NAME_LEN - 1);
    m_currentProfile[PROFILE_NAME_LEN - 1] = '\0';
    return true;
}

bool ConfigManager::listProfiles(char** out, int maxCount, int& outCount) {
    if (!m_running || !out || maxCount <= 0) return false;

    outCount = 0;
    File dir = LittleFS.open(PROFILES_DIR);
    if (!dir) return false;

    File entry = dir.openNextFile();
    while (entry && outCount < maxCount) {
        const char* name = entry.name();
        if (strstr(name, ".json")) {
            char* copy = strdup(name);
            if (copy) {
                char* dot = strrchr(copy, '.');
                if (dot) *dot = '\0';
                const char* slash = strrchr(copy, '/');
                out[outCount] = strdup(slash ? slash + 1 : copy);
                free(copy);
                outCount++;
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
    return true;
}
