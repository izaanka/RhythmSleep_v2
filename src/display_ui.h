#pragma once

#include <Arduino.h>
#include "config.h"

// Forward declarations
struct EEGFeatures;
struct BandPowers;
struct SleepEpochData;

class DisplayUI {
public:
    void begin();
    void update();
    void setScreen(MenuScreen screen);
    MenuScreen getScreen();
    void nextScreen();
    void prevScreen();
    
    void handleButton(ButtonEvent event);
    
    void setWaveformSample(float sample);
    void setBandPowers(const BandPowers& bp);
    void setSleepStage(SleepStage stage, float confidence);
    void setTime(uint8_t h, uint8_t m, uint8_t s);
    void setAlarmWindow(uint8_t minH, uint8_t minM, uint8_t maxH, uint8_t maxM);
    void setHypnogram(const SleepEpochData* data, int count);
    
private:
    void drawHomeScreen();
    void drawWaveformScreen();
    void drawBandPowerScreen();
    void drawSettingsScreen();
    void drawSleepSummaryScreen();
    void drawHeader(const char* title);
    void drawStatusBar();
    
    MenuScreen currentScreen = MenuScreen::HOME;
    bool needsFullRedraw = true;
    unsigned long lastDrawMs = 0;
    
    float waveformBuf[TFT_SCREEN_WIDTH];
    int waveformIdx = 0;
    int waveformCount = 0;
    
    BandPowers displayBands;
    SleepStage displayStage = STAGE_UNKNOWN;
    float displayConfidence = 0;
    uint8_t dispHour = 0, dispMin = 0, dispSec = 0;
    uint8_t alarmMinH = 6, alarmMinM = 30, alarmMaxH = 7, alarmMaxM = 30;
    
    int settingsField = 0;
    bool settingsEditing = false;
    uint8_t editMinH = 6, editMinM = 30, editMaxH = 7, editMaxM = 30;
    
    const SleepEpochData* hypnogramData = nullptr;
    int hypnogramCount = 0;
};
