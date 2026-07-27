#pragma once

#include "config.h"
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

class SDManager {
public:
    bool begin();                          // Init SPI SD card on shared bus
    
    // Configuration
    bool loadConfig(UserConfig& config);   // Read config.json into struct
    bool saveConfig(const UserConfig& config); // Write struct to config.json
    
    // Neural network weights
    bool loadWeights(const char* path, float* buffer, size_t count); // Read binary floats
    
    // Sleep logging
    bool logSleepEpoch(const char* dateStr, uint32_t timestamp, SleepStage stage,
                       float confidence, const BandPowers& bands);
    String getLatestSleepLogPath();        // Path to most recent log file
    String readSleepLog(const char* path); // Read entire log file as string
    bool listSleepLogs(String* paths, int maxCount, int& outCount); // List log files
    
    // Raw EEG (optional)
    bool logRawEEG(const char* dateStr, const float* samples, int count);
    
    bool isReady();
    
private:
    bool ready = false;
    void ensureDirectory(const char* path);
};
