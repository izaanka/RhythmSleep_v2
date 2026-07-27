#include "alarm_controller.h"
#include <DFRobotDFPlayerMini.h>

static DFRobotDFPlayerMini myDFPlayer;

void AlarmController::begin() {
    Serial1.begin(9600, SERIAL_8N1, PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
    
    if (!myDFPlayer.begin(Serial1)) {
        Serial.println("DFPlayer Mini failed to begin!");
        dfPlayerReady = false;
    } else {
        dfPlayerReady = true;
        myDFPlayer.volume(currentVolume);
    }
    
    ledcAttach(PIN_VIBRATION, VIB_PWM_FREQ, VIB_PWM_RESOLUTION);
    ledcWrite(PIN_VIBRATION, 0);
}

void AlarmController::update() {
    if (snoozed) {
        if (millis() >= snoozeUntilMs) {
            snoozed = false;
            startAlarm();
        }
        return;
    }
    
    if (!alarming) return;
    
    unsigned long elapsed = millis() - alarmStartMs;
    
    // Update volume ramp
    if (elapsed < ALARM_ESCALATION_MS) {
        int newVolume = ALARM_VOLUME_MIN + ((ALARM_VOLUME_MAX - ALARM_VOLUME_MIN) * elapsed) / ALARM_RAMP_MS;
        if (newVolume > ALARM_VOLUME_MAX) newVolume = ALARM_VOLUME_MAX;
        
        if (newVolume != currentVolume) {
            currentVolume = newVolume;
            if (dfPlayerReady) myDFPlayer.volume(currentVolume);
        }
        
        // Pulse vibration
        if (vibState) {
            if (millis() - lastVibToggle >= VIB_PULSE_ON_MS) {
                vibState = false;
                ledcWrite(PIN_VIBRATION, 0);
                lastVibToggle = millis();
            }
        } else {
            if (millis() - lastVibToggle >= VIB_PULSE_OFF_MS) {
                vibState = true;
                ledcWrite(PIN_VIBRATION, 128); // 50% duty
                lastVibToggle = millis();
            }
        }
    } else {
        // Max intensity
        if (currentVolume != ALARM_VOLUME_MAX) {
            currentVolume = ALARM_VOLUME_MAX;
            if (dfPlayerReady) myDFPlayer.volume(currentVolume);
        }
        ledcWrite(PIN_VIBRATION, 255); // Continuous on
    }
}

void AlarmController::startAlarm() {
    if (alarming) return;
    alarming = true;
    snoozed = false;
    alarmStartMs = millis();
    currentVolume = ALARM_VOLUME_MIN;
    
    if (dfPlayerReady) {
        myDFPlayer.volume(currentVolume);
        myDFPlayer.loop(1);
    }
    
    vibState = true;
    lastVibToggle = millis();
    ledcWrite(PIN_VIBRATION, 128); // 50% duty
}

void AlarmController::stopAlarm() {
    alarming = false;
    snoozed = false;
    
    if (dfPlayerReady) myDFPlayer.pause();
    ledcWrite(PIN_VIBRATION, 0);
}

void AlarmController::snooze(int minutes) {
    if (alarming) {
        alarming = false;
        snoozed = true;
        snoozeUntilMs = millis() + (minutes * 60000UL);
        
        if (dfPlayerReady) myDFPlayer.pause();
        ledcWrite(PIN_VIBRATION, 0);
    }
}

bool AlarmController::isAlarming() {
    return alarming;
}

bool AlarmController::isSnoozed() {
    return snoozed;
}
