#include "sd_manager.h"

extern SemaphoreHandle_t spiMutex;

SPIClass sdSPI(FSPI);

bool SDManager::begin() {
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        sdSPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
        if (!SD.begin(PIN_SD_CS, sdSPI)) {
            Serial.println("SD Card initialization failed!");
            ready = false;
            xSemaphoreGive(spiMutex);
            return false;
        }
        ready = true;
        
        ensureDirectory(SD_SLEEP_LOG_DIR);
        ensureDirectory(SD_RAW_EEG_DIR);
        
        xSemaphoreGive(spiMutex);
        return true;
    }
    return false;
}

void SDManager::ensureDirectory(const char* path) {
    if (!SD.exists(path)) {
        SD.mkdir(path);
    }
}

bool SDManager::isReady() {
    return ready;
}

bool SDManager::loadConfig(UserConfig& config) {
    if (!ready) return false;
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        File file = SD.open(SD_CONFIG_PATH, FILE_READ);
        if (!file) {
            xSemaphoreGive(spiMutex);
            return false;
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        
        if (!error) {
            // User section
            if (doc.containsKey("user")) {
                JsonObject obj = doc["user"];
                strlcpy(config.name, obj["name"] | "User", sizeof(config.name));
                config.age = obj["age"] | 25;
                strlcpy(config.gender, obj["gender"] | "male", sizeof(config.gender));
            } else {
                strlcpy(config.name, doc["name"] | "User", sizeof(config.name));
                config.age = doc["age"] | 25;
                strlcpy(config.gender, doc["gender"] | "male", sizeof(config.gender));
            }

            // Alarm section
            if (doc.containsKey("alarm")) {
                JsonObject obj = doc["alarm"];
                config.minWakeHour = obj["min_wake_hour"] | obj["minWakeHour"] | 6;
                config.minWakeMinute = obj["min_wake_minute"] | obj["minWakeMinute"] | 30;
                config.maxWakeHour = obj["max_wake_hour"] | obj["maxWakeHour"] | 7;
                config.maxWakeMinute = obj["max_wake_minute"] | obj["maxWakeMinute"] | 30;
                config.lightSleepThresh = obj["light_sleep_threshold"] | obj["lightSleepThresh"] | DEFAULT_LIGHT_THRESH;
                config.alarmVolume = obj["alarm_volume"] | obj["alarmVolume"] | 20;
            } else {
                config.minWakeHour = doc["minWakeHour"] | 6;
                config.minWakeMinute = doc["minWakeMinute"] | 30;
                config.maxWakeHour = doc["maxWakeHour"] | 7;
                config.maxWakeMinute = doc["maxWakeMinute"] | 30;
                config.lightSleepThresh = doc["lightSleepThresh"] | DEFAULT_LIGHT_THRESH;
                config.alarmVolume = doc["alarmVolume"] | 20;
            }

            // WiFi section
            if (doc.containsKey("wifi")) {
                JsonObject obj = doc["wifi"];
                strlcpy(config.wifiSSID, obj["ssid"] | obj["wifiSSID"] | "RhythmSleep", sizeof(config.wifiSSID));
                strlcpy(config.wifiPassword, obj["password"] | obj["wifiPassword"] | "sleep1234", sizeof(config.wifiPassword));
                config.apMode = obj["ap_mode"] | obj["apMode"] | true;
            } else {
                strlcpy(config.wifiSSID, doc["wifiSSID"] | "RhythmSleep", sizeof(config.wifiSSID));
                strlcpy(config.wifiPassword, doc["wifiPassword"] | "sleep1234", sizeof(config.wifiPassword));
                config.apMode = doc["apMode"] | true;
            }

            // Recording section
            if (doc.containsKey("recording")) {
                JsonObject obj = doc["recording"];
                config.saveRawEEG = obj["save_raw_eeg"] | obj["saveRawEEG"] | false;
            } else {
                config.saveRawEEG = doc["saveRawEEG"] | false;
            }
        }
        
        xSemaphoreGive(spiMutex);
        return error == DeserializationError::Ok;
    }
    return false;
}

bool SDManager::saveConfig(const UserConfig& config) {
    if (!ready) return false;
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        File file = SD.open(SD_CONFIG_PATH, FILE_WRITE);
        if (!file) {
            xSemaphoreGive(spiMutex);
            return false;
        }
        
        JsonDocument doc;
        doc["name"] = config.name;
        doc["age"] = config.age;
        doc["gender"] = config.gender;
        doc["minWakeHour"] = config.minWakeHour;
        doc["minWakeMinute"] = config.minWakeMinute;
        doc["maxWakeHour"] = config.maxWakeHour;
        doc["maxWakeMinute"] = config.maxWakeMinute;
        doc["lightSleepThresh"] = config.lightSleepThresh;
        doc["alarmVolume"] = config.alarmVolume;
        doc["wifiSSID"] = config.wifiSSID;
        doc["wifiPassword"] = config.wifiPassword;
        doc["apMode"] = config.apMode;
        doc["saveRawEEG"] = config.saveRawEEG;
        
        serializeJson(doc, file);
        file.close();
        
        xSemaphoreGive(spiMutex);
        return true;
    }
    return false;
}

bool SDManager::loadWeights(const char* path, float* buffer, size_t count) {
    if (!ready) return false;
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        File file = SD.open(path, FILE_READ);
        if (!file) {
            xSemaphoreGive(spiMutex);
            return false;
        }
        size_t bytesRead = file.read((uint8_t*)buffer, count * sizeof(float));
        file.close();
        xSemaphoreGive(spiMutex);
        return bytesRead == count * sizeof(float);
    }
    return false;
}

bool SDManager::logSleepEpoch(const char* dateStr, uint32_t timestamp, SleepStage stage,
                               float confidence, const BandPowers& bands) {
    if (!ready) return false;
    String filepath = String(SD_SLEEP_LOG_DIR) + "/" + dateStr + ".csv";
    
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        bool isNewFile = !SD.exists(filepath);
        File file = SD.open(filepath, FILE_APPEND);
        if (!file) {
            xSemaphoreGive(spiMutex);
            return false;
        }
        
        if (isNewFile) {
            file.println("timestamp,stage,confidence,delta,theta,alpha,beta,gamma");
        }
        
        file.print(timestamp);
        file.print(",");
        file.print((int)stage);
        file.print(",");
        file.print(confidence);
        file.print(",");
        file.print(bands.delta);
        file.print(",");
        file.print(bands.theta);
        file.print(",");
        file.print(bands.alpha);
        file.print(",");
        file.print(bands.beta);
        file.print(",");
        file.println(bands.gamma);
        
        file.close();
        xSemaphoreGive(spiMutex);
        return true;
    }
    return false;
}

String SDManager::getLatestSleepLogPath() {
    String latestPath = "";
    if (!ready) return latestPath;
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        File dir = SD.open(SD_SLEEP_LOG_DIR);
        if (dir) {
            String latestName = "";
            while (true) {
                File entry = dir.openNextFile();
                if (!entry) break;
                if (!entry.isDirectory()) {
                    String name = entry.name();
                    if (name.endsWith(".csv")) {
                        if (name > latestName) {
                            latestName = name;
                        }
                    }
                }
                entry.close();
            }
            if (latestName.length() > 0) {
                latestPath = String(SD_SLEEP_LOG_DIR) + "/" + latestName;
            }
            dir.close();
        }
        xSemaphoreGive(spiMutex);
    }
    return latestPath;
}

String SDManager::readSleepLog(const char* path) {
    String content = "";
    if (!ready) return content;
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        File file = SD.open(path, FILE_READ);
        if (file) {
            while (file.available()) {
                content += (char)file.read();
            }
            file.close();
        }
        xSemaphoreGive(spiMutex);
    }
    return content;
}

bool SDManager::listSleepLogs(String* paths, int maxCount, int& outCount) {
    outCount = 0;
    if (!ready) return false;
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        File dir = SD.open(SD_SLEEP_LOG_DIR);
        if (dir) {
            while (outCount < maxCount) {
                File entry = dir.openNextFile();
                if (!entry) break;
                if (!entry.isDirectory()) {
                    String name = entry.name();
                    if (name.endsWith(".csv")) {
                        paths[outCount++] = String(SD_SLEEP_LOG_DIR) + "/" + name;
                    }
                }
                entry.close();
            }
            dir.close();
        }
        xSemaphoreGive(spiMutex);
        return true;
    }
    return false;
}

bool SDManager::logRawEEG(const char* dateStr, const float* samples, int count) {
    if (!ready) return false;
    String filepath = String(SD_RAW_EEG_DIR) + "/" + dateStr + "_raw.bin";
    
    if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
        File file = SD.open(filepath, FILE_APPEND);
        if (!file) {
            xSemaphoreGive(spiMutex);
            return false;
        }
        file.write((const uint8_t*)samples, count * sizeof(float));
        file.close();
        xSemaphoreGive(spiMutex);
        return true;
    }
    return false;
}
