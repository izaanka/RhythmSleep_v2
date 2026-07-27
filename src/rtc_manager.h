#pragma once

#include "config.h"
#include <RTClib.h>
#include <Wire.h>

class RTCManager {
public:
    bool begin();                          // Init I2C on PIN_RTC_SDA/SCL, detect PCF8563
    void update();                         // Read current time from RTC
    
    uint8_t getHour();                     // Current hour (0-23)
    uint8_t getMinute();                   // Current minute (0-59)
    uint8_t getSecond();                   // Current second (0-59)
    uint32_t getUnixTime();                // Unix timestamp
    String getTimeString();                // "HH:MM:SS"
    String getDateString();                // "YYYY-MM-DD"
    String getDateTimeString();            // "YYYY-MM-DD HH:MM:SS"
    
    void setTime(int year, int month, int day, int hour, int minute, int second);
    
    // Wake window check
    bool isInWakeWindow(uint8_t minH, uint8_t minM, uint8_t maxH, uint8_t maxM);
    bool isPastMaxWake(uint8_t maxH, uint8_t maxM);
    
private:
    uint8_t hour = 0, minute = 0, second = 0;
    uint16_t year = 2025;
    uint8_t month = 1, day = 1;
    uint32_t unixTime = 0;
    bool rtcReady = false;

    RTC_PCF8563 rtc;
};
