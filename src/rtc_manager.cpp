#include "rtc_manager.h"

bool RTCManager::begin() {
    Wire.begin(PIN_RTC_SDA, PIN_RTC_SCL);
    if (!rtc.begin(&Wire)) {
        Serial.println("Couldn't find RTC");
        rtcReady = false;
        return false;
    }
    
    rtcReady = true;
    if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return true;
}

void RTCManager::update() {
    if (rtcReady) {
        DateTime now = rtc.now();
        year = now.year();
        month = now.month();
        day = now.day();
        hour = now.hour();
        minute = now.minute();
        second = now.second();
        unixTime = now.unixtime();
    } else {
        // System uptime fallback when RTC is unavailable
        unsigned long sec = millis() / 1000;
        second = sec % 60;
        minute = (sec / 60) % 60;
        hour = (sec / 3600) % 24;
        unixTime = sec;
    }
}

uint8_t RTCManager::getHour() { return hour; }
uint8_t RTCManager::getMinute() { return minute; }
uint8_t RTCManager::getSecond() { return second; }
uint32_t RTCManager::getUnixTime() { return unixTime; }

String RTCManager::getTimeString() {
    char buf[10];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
    return String(buf);
}

String RTCManager::getDateString() {
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    return String(buf);
}

String RTCManager::getDateTimeString() {
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    return String(buf);
}

void RTCManager::setTime(int y, int m, int d, int h, int min, int s) {
    rtc.adjust(DateTime(y, m, d, h, min, s));
    update();
}

bool RTCManager::isInWakeWindow(uint8_t minH, uint8_t minM, uint8_t maxH, uint8_t maxM) {
    int currentMins = hour * 60 + minute;
    int minMins = minH * 60 + minM;
    int maxMins = maxH * 60 + maxM;
    
    if (minMins <= maxMins) {
        return (currentMins >= minMins && currentMins <= maxMins);
    } else {
        return (currentMins >= minMins || currentMins <= maxMins);
    }
}

bool RTCManager::isPastMaxWake(uint8_t maxH, uint8_t maxM) {
    int currentMins = hour * 60 + minute;
    int maxMins = maxH * 60 + maxM;
    return currentMins >= maxMins;
}
