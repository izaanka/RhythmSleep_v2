#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <functional>
#include "config.h"

class WebDashboard {
public:
    void begin(UserConfig& config);
    void update();
    
    void setSleepData(const SleepEpochData* data, int count);
    void setCurrentStage(SleepStage stage, const float* confidences);
    void setBandPowers(const BandPowers& bp);
    void setFilteredSample(float sample);
    
    void setConfigCallback(std::function<void(const UserConfig&)> cb);
    
    String getIPAddress();
    
private:
    void setupRoutes();
    
    UserConfig* configPtr = nullptr;
    std::function<void(const UserConfig&)> onConfigChange = nullptr;
    
    SleepStage currentStage = STAGE_UNKNOWN;
    float confidences[NN_OUTPUT_SIZE] = {0};
    BandPowers bands;
    float latestSample = 0;
    const SleepEpochData* sleepData = nullptr;
    int sleepDataCount = 0;
    
    WebServer server{WEB_SERVER_PORT};
};

extern const char DASHBOARD_HTML[] PROGMEM;
