#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
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
    void handleConfigPost(AsyncWebServerRequest* req, uint8_t* data, size_t len);
    void sendWebSocketUpdate();
    
    UserConfig* configPtr = nullptr;
    std::function<void(const UserConfig&)> onConfigChange = nullptr;
    
    SleepStage currentStage = STAGE_UNKNOWN;
    float confidences[NN_OUTPUT_SIZE] = {0};
    BandPowers bands;
    float latestSample = 0;
    const SleepEpochData* sleepData = nullptr;
    int sleepDataCount = 0;
    
    unsigned long lastWsUpdate = 0;
    
    AsyncWebServer server{WEB_SERVER_PORT};
    AsyncWebSocket ws{WEBSOCKET_PATH};
};

extern const char DASHBOARD_HTML[] PROGMEM;
