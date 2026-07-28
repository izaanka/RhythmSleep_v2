#pragma once

#include "config.h"

class AlarmController {
public:
    void begin();                          // Init DFPlayer (UART1) + vibration motor PWM
    void update();                         // Call every loop — handles ramping, pulsing
    
    void startAlarm();                     // Begin alarm sequence
    void stopAlarm();                      // Stop everything
    void snooze(int minutes = 5);          // Pause for N minutes then restart
    
    bool isAlarming();                     // Currently in alarm mode
    bool isSnoozed();                      // Currently in snooze
    
private:
    void setVibrationDuty(uint8_t duty);
    bool alarming = false;
    bool snoozed = false;
    bool dfPlayerReady = false;
    unsigned long alarmStartMs = 0;
    unsigned long snoozeUntilMs = 0;
    unsigned long lastVibToggle = 0;
    bool vibState = false;
    int currentVolume = ALARM_VOLUME_MIN;
};
