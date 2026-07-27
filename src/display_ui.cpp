#include "display_ui.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void DisplayUI::begin() {
    pinMode(PIN_TFT_BLK, OUTPUT);
    digitalWrite(PIN_TFT_BLK, HIGH);
    
    if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
        tft.begin();
        tft.setRotation(1); // Landscape
        tft.fillScreen(COLOR_BG);
        xSemaphoreGive(spiMutex);
    }
}

void DisplayUI::update() {
    unsigned long now = millis();
    if (now - lastDrawMs < 100 && currentScreen != MenuScreen::WAVEFORM) return;
    
    if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
        if (needsFullRedraw) {
            tft.fillScreen(COLOR_BG);
            needsFullRedraw = false;
        }
        
        switch (currentScreen) {
            case MenuScreen::HOME: drawHomeScreen(); break;
            case MenuScreen::WAVEFORM: drawWaveformScreen(); break;
            case MenuScreen::BAND_POWERS: drawBandPowerScreen(); break;
            case MenuScreen::SETTINGS_ALARM: drawSettingsScreen(); break;
            case MenuScreen::SLEEP_SUMMARY: drawSleepSummaryScreen(); break;
            default: break;
        }
        
        if (currentScreen != MenuScreen::WAVEFORM) {
            lastDrawMs = now;
        }
        xSemaphoreGive(spiMutex);
    }
}

void DisplayUI::setScreen(MenuScreen screen) {
    if (currentScreen != screen) {
        currentScreen = screen;
        needsFullRedraw = true;
    }
}

MenuScreen DisplayUI::getScreen() {
    return currentScreen;
}

void DisplayUI::nextScreen() {
    int next = (int)currentScreen + 1;
    if (next >= (int)MenuScreen::SCREEN_COUNT) next = 0;
    setScreen((MenuScreen)next);
}

void DisplayUI::prevScreen() {
    int prev = (int)currentScreen - 1;
    if (prev < 0) prev = (int)MenuScreen::SCREEN_COUNT - 1;
    setScreen((MenuScreen)prev);
}

void DisplayUI::handleButton(ButtonEvent event) {
    if (event == ButtonEvent::PRESS_BACK) {
        if (currentScreen == MenuScreen::SETTINGS_ALARM && settingsEditing) {
            settingsEditing = false;
        } else if (currentScreen == MenuScreen::SETTINGS_ALARM) {
            alarmMinH = editMinH; alarmMinM = editMinM;
            alarmMaxH = editMaxH; alarmMaxM = editMaxM;
            setScreen(MenuScreen::HOME);
        } else {
            setScreen(MenuScreen::HOME);
        }
        needsFullRedraw = true;
        return;
    }
    
    if (currentScreen == MenuScreen::HOME) {
        if (event == ButtonEvent::PRESS_UP) prevScreen();
        else if (event == ButtonEvent::PRESS_DOWN) nextScreen();
    } else if (currentScreen == MenuScreen::SETTINGS_ALARM) {
        if (event == ButtonEvent::PRESS_SELECT) {
            settingsEditing = !settingsEditing;
            if (!settingsEditing) settingsField = (settingsField + 1) % 4;
            needsFullRedraw = true;
        } else if (settingsEditing) {
            if (event == ButtonEvent::PRESS_UP) {
                if (settingsField == 0) editMinH = (editMinH + 1) % 24;
                else if (settingsField == 1) editMinM = (editMinM + 1) % 60;
                else if (settingsField == 2) editMaxH = (editMaxH + 1) % 24;
                else if (settingsField == 3) editMaxM = (editMaxM + 1) % 60;
                needsFullRedraw = true;
            } else if (event == ButtonEvent::PRESS_DOWN) {
                if (settingsField == 0) editMinH = (editMinH + 23) % 24;
                else if (settingsField == 1) editMinM = (editMinM + 59) % 60;
                else if (settingsField == 2) editMaxH = (editMaxH + 23) % 24;
                else if (settingsField == 3) editMaxM = (editMaxM + 59) % 60;
                needsFullRedraw = true;
            }
        }
    } else {
        if (event == ButtonEvent::PRESS_UP) prevScreen();
        else if (event == ButtonEvent::PRESS_DOWN) nextScreen();
    }
}

void DisplayUI::setWaveformSample(float sample) {
    waveformBuf[waveformIdx] = sample;
    waveformIdx = (waveformIdx + 1) % TFT_SCREEN_WIDTH;
    if (waveformCount < TFT_SCREEN_WIDTH) waveformCount++;
}

void DisplayUI::setBandPowers(const BandPowers& bp) {
    displayBands = bp;
}

void DisplayUI::setSleepStage(SleepStage stage, float confidence) {
    displayStage = stage;
    displayConfidence = confidence;
}

void DisplayUI::setTime(uint8_t h, uint8_t m, uint8_t s) {
    dispHour = h; dispMin = m; dispSec = s;
}

void DisplayUI::setAlarmWindow(uint8_t minH, uint8_t minM, uint8_t maxH, uint8_t maxM) {
    alarmMinH = minH; alarmMinM = minM; alarmMaxH = maxH; alarmMaxM = maxM;
    editMinH = minH; editMinM = minM; editMaxH = maxH; editMaxM = maxM;
}

void DisplayUI::setHypnogram(const SleepEpochData* data, int count) {
    hypnogramData = data;
    hypnogramCount = count;
}

void DisplayUI::drawHeader(const char* title) {
    tft.fillRect(0, 0, TFT_SCREEN_WIDTH, 20, COLOR_GRID);
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.drawString(title, TFT_SCREEN_WIDTH / 2, 10);
}

void DisplayUI::drawStatusBar() {
    tft.fillRect(0, TFT_SCREEN_HEIGHT - 20, TFT_SCREEN_WIDTH, 20, COLOR_GRID);
    char buf[16];
    sprintf(buf, "%02d:%02d:%02d", dispHour, dispMin, dispSec);
    tft.setTextColor(COLOR_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.drawString(buf, TFT_SCREEN_WIDTH / 2, TFT_SCREEN_HEIGHT - 10);
}

void DisplayUI::drawHomeScreen() {
    // Header
    tft.setTextFont(2);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_ACCENT);
    tft.drawString("RhythmSleep", TFT_SCREEN_WIDTH / 2, 10);
    
    // Clock
    char timeStr[16];
    sprintf(timeStr, "%02d:%02d:%02d", dispHour, dispMin, dispSec);
    tft.setTextFont(7);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString(timeStr, TFT_SCREEN_WIDTH / 2, 80);
    
    // Stage
    const char* stageStr = sleepStageStr(displayStage);
    uint16_t stageCol = sleepStageColor(displayStage);
    
    int w = tft.textWidth(stageStr, 4) + 20;
    tft.fillRoundRect(TFT_SCREEN_WIDTH/2 - w/2, 130, w, 30, 15, stageCol);
    tft.setTextColor(COLOR_BG);
    tft.setTextFont(4);
    tft.drawString(stageStr, TFT_SCREEN_WIDTH/2, 145);
    
    // Confidence
    tft.drawRect(TFT_SCREEN_WIDTH/2 - 50, 170, 100, 6, COLOR_TEXT_DIM);
    tft.fillRect(TFT_SCREEN_WIDTH/2 - 49, 171, 98, 4, COLOR_BG);
    tft.fillRect(TFT_SCREEN_WIDTH/2 - 49, 171, (int)(98.0f * displayConfidence), 4, stageCol);
    
    // Alarm
    char alarmStr[32];
    sprintf(alarmStr, "Wake: %02d:%02d - %02d:%02d", alarmMinH, alarmMinM, alarmMaxH, alarmMaxM);
    tft.setTextFont(2);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString(alarmStr, TFT_SCREEN_WIDTH/2, 200);
}

void DisplayUI::drawWaveformScreen() {
    if (needsFullRedraw) {
        drawHeader("EEG Waveform");
        tft.drawLine(0, 130, TFT_SCREEN_WIDTH, 130, COLOR_GRID); // center line
        
        // Redraw complete waveform buffer on full redraw
        int pX = 0;
        int pY = 130 - (int)(waveformBuf[0] * 90.0f / 500.0f);
        if (pY < 20) pY = 20; if (pY > 239) pY = 239;
        for (int x = 1; x < TFT_SCREEN_WIDTH; x++) {
            int cY = 130 - (int)(waveformBuf[x] * 90.0f / 500.0f);
            if (cY < 20) cY = 20; if (cY > 239) cY = 239;
            tft.drawLine(pX, pY, x, cY, COLOR_WAVEFORM);
            pX = x;
            pY = cY;
        }
    }
    
    // Incrementally draw waveform
    static int lastX = 0;
    static int lastY = 130;
    
    int currentX = waveformIdx - 1;
    if (currentX < 0) currentX = TFT_SCREEN_WIDTH - 1;
    
    float val = waveformBuf[currentX];
    // Map -500..500 to y
    int y = 130 - (int)(val * 90.0f / 500.0f);
    if (y < 20) y = 20;
    if (y > 239) y = 239;
    
    // Clear ahead
    int clearX = (currentX + 5) % TFT_SCREEN_WIDTH;
    tft.drawFastVLine(clearX, 20, 220, COLOR_BG);
    if (clearX % 20 == 0) tft.drawFastVLine(clearX, 20, 220, COLOR_GRID);
    tft.drawPixel(clearX, 130, COLOR_TEXT_DIM);
    
    if (currentX > 0 && currentX != lastX + 1) {
        // wrap around, no line
    } else {
        tft.drawLine(lastX, lastY, currentX, y, COLOR_WAVEFORM);
    }
    
    lastX = currentX;
    lastY = y;
}

void DisplayUI::drawBandPowerScreen() {
    drawHeader("Band Powers");
    
    const char* labels[] = {"Delta", "Theta", "Alpha", "Beta", "Gamma"};
    float values[] = {displayBands.relDelta, displayBands.relTheta, displayBands.relAlpha, displayBands.relBeta, displayBands.relGamma};
    uint16_t colors[] = {COLOR_DELTA, COLOR_THETA, COLOR_ALPHA, COLOR_BETA, COLOR_GAMMA};
    
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    
    for (int i=0; i<5; i++) {
        int y = 40 + i * 35;
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString(labels[i], 10, y);
        
        tft.fillRect(70, y - 10, 200, 20, COLOR_BAR_BG);
        tft.fillRect(70, y - 10, 200 * values[i], 20, colors[i]);
        
        char pct[16];
        sprintf(pct, "%3d%%", (int)(values[i] * 100));
        tft.drawString(pct, 275, y);
    }
}

void DisplayUI::drawSettingsScreen() {
    drawHeader("Alarm Settings");
    
    tft.setTextFont(4);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString("Min Wake:", 140, 70);
    tft.drawString("Max Wake:", 140, 130);
    
    tft.setTextDatum(ML_DATUM);
    char buf[8];
    
    // Min H
    sprintf(buf, "%02d", editMinH);
    tft.setTextColor((settingsField == 0) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
    if (settingsField == 0 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
    tft.drawString(buf, 150, 70);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString(":", 185, 70);
    
    // Min M
    sprintf(buf, "%02d", editMinM);
    tft.setTextColor((settingsField == 1) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
    if (settingsField == 1 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
    tft.drawString(buf, 200, 70);
    
    // Max H
    sprintf(buf, "%02d", editMaxH);
    tft.setTextColor((settingsField == 2) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
    if (settingsField == 2 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
    tft.drawString(buf, 150, 130);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString(":", 185, 130);
    
    // Max M
    sprintf(buf, "%02d", editMaxM);
    tft.setTextColor((settingsField == 3) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
    if (settingsField == 3 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
    tft.drawString(buf, 200, 130);
    
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString("UP/DOWN: Adjust  SEL: Edit  BACK: Save", TFT_SCREEN_WIDTH/2, 210);
}

void DisplayUI::drawSleepSummaryScreen() {
    drawHeader("Last Night");
    
    if (hypnogramCount == 0 || !hypnogramData) {
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM);
        tft.drawString("No sleep data available", TFT_SCREEN_WIDTH/2, 120);
        return;
    }
    
    // Hypnogram
    int hWidth = 280;
    int hX = 20;
    int hY = 50;
    int hHeight = 30;
    tft.drawRect(hX-1, hY-1, hWidth+2, hHeight+2, COLOR_GRID);
    
    int wakeCnt = 0, lightCnt = 0, deepCnt = 0, remCnt = 0;
    
    for (int i=0; i<hypnogramCount; i++) {
        int x1 = hX + (i * hWidth) / hypnogramCount;
        int x2 = hX + ((i+1) * hWidth) / hypnogramCount;
        int w = x2 - x1;
        if (w == 0) w = 1;
        
        uint16_t c = sleepStageColor(hypnogramData[i].stage);
        tft.fillRect(x1, hY, w, hHeight, c);
        
        switch (hypnogramData[i].stage) {
            case STAGE_WAKE: wakeCnt++; break;
            case STAGE_LIGHT: lightCnt++; break;
            case STAGE_DEEP: deepCnt++; break;
            case STAGE_REM: remCnt++; break;
            default: break;
        }
    }
    
    float epochMins = EEG_EPOCH_SECONDS / 60.0f;
    int totalSleepMins = (lightCnt + deepCnt + remCnt) * epochMins;
    int totalMins = (wakeCnt + lightCnt + deepCnt + remCnt) * epochMins;
    int eff = (totalMins > 0) ? (totalSleepMins * 100 / totalMins) : 0;
    
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT);
    
    char buf[64];
    sprintf(buf, "WAKE: %dm", (int)(wakeCnt * epochMins));
    tft.setTextColor(COLOR_WAKE); tft.drawString(buf, 20, 100);
    
    sprintf(buf, "LIGHT: %dm", (int)(lightCnt * epochMins));
    tft.setTextColor(COLOR_LIGHT); tft.drawString(buf, 20, 130);
    
    sprintf(buf, "DEEP: %dm", (int)(deepCnt * epochMins));
    tft.setTextColor(COLOR_DEEP); tft.drawString(buf, 160, 100);
    
    sprintf(buf, "REM: %dm", (int)(remCnt * epochMins));
    tft.setTextColor(COLOR_REM); tft.drawString(buf, 160, 130);
    
    tft.setTextColor(COLOR_TEXT);
    sprintf(buf, "Efficiency: %d%%", eff);
    tft.drawString(buf, 20, 170);
    sprintf(buf, "Total: %dh %dm", totalSleepMins / 60, totalSleepMins % 60);
    tft.drawString(buf, 160, 170);
}
