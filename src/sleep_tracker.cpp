// =====================================================================
// RhythmSleep — Sleep Stage Tracker & Wake-Up Decision Engine
//
// Maintains a rolling hypnogram of classified sleep epochs, computes
// sleep statistics, and decides when to trigger the alarm based on
// prolonged light sleep within the user's configured wake window.
// =====================================================================

#include "sleep_tracker.h"
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────
// Initialisation
// ─────────────────────────────────────────────────────────────────────

void SleepTracker::begin() {
    reset();
    Serial.println("[SleepTracker] Initialised");
}

void SleepTracker::reset() {
    epochCount       = 0;
    consecutiveLight = 0;
    sleepOnsetIndex  = -1;
    sleepOnsetFound  = false;
    memset(stageEpochs, 0, sizeof(stageEpochs));
    memset(hypnogram, 0, sizeof(SleepEpochData) * MAX_HYPNOGRAM_LEN);
}

// ─────────────────────────────────────────────────────────────────────
// Epoch Recording
// ─────────────────────────────────────────────────────────────────────

void SleepTracker::recordEpoch(SleepStage stage, float confidence,
                                const BandPowers& bands, uint32_t timestamp) {
    if (epochCount >= MAX_HYPNOGRAM_LEN) {
        // Shift hypnogram elements left by 1 to maintain rolling 12-hour buffer
        memmove(&hypnogram[0], &hypnogram[1], sizeof(SleepEpochData) * (MAX_HYPNOGRAM_LEN - 1));
        epochCount = MAX_HYPNOGRAM_LEN - 1;
    }

    // Store epoch data
    SleepEpochData& e = hypnogram[epochCount];
    e.timestamp  = timestamp;
    e.stage      = stage;
    e.confidence = confidence;
    e.bands      = bands;

    // Update stage counters
    if (stage < 4) {
        stageEpochs[stage]++;
    }

    // Track consecutive light-sleep epochs for wake-up decision
    if (stage == STAGE_LIGHT) {
        consecutiveLight++;
    } else {
        consecutiveLight = 0;
    }

    // Detect sleep onset: first run of SLEEP_ONSET_THRESH consecutive non-wake epochs
    if (!sleepOnsetFound && stage != STAGE_WAKE) {
        // Count backwards from current epoch to see if we have enough consecutive non-wake
        int run = 0;
        for (int i = epochCount; i >= 0 && i > epochCount - SLEEP_ONSET_THRESH; i--) {
            if (hypnogram[i].stage != STAGE_WAKE) {
                run++;
            } else {
                break;
            }
        }
        if (run >= SLEEP_ONSET_THRESH) {
            sleepOnsetIndex = epochCount - SLEEP_ONSET_THRESH + 1;
            sleepOnsetFound = true;
            Serial.printf("[SleepTracker] Sleep onset detected at epoch %d\n", sleepOnsetIndex);
        }
    }

    epochCount++;

    Serial.printf("[SleepTracker] Epoch %d: %s (%.0f%%) | Light streak: %d\n",
                  epochCount, sleepStageStr(stage), confidence * 100.0f, consecutiveLight);
}

// ─────────────────────────────────────────────────────────────────────
// Wake-Up Decision
// ─────────────────────────────────────────────────────────────────────

bool SleepTracker::shouldWakeUp(uint8_t currentHour, uint8_t currentMinute) const {
    // Convert times to minutes-since-midnight for easy comparison
    int nowMins = currentHour * 60 + currentMinute;
    int minMins = globalConfig.minWakeHour * 60 + globalConfig.minWakeMinute;
    int maxMins = globalConfig.maxWakeHour * 60 + globalConfig.maxWakeMinute;

    // Handle midnight-crossing windows (e.g., 23:00 – 01:00)
    bool inWindow;
    if (minMins <= maxMins) {
        // Normal window: min < max (e.g., 06:30 – 07:30)
        inWindow = (nowMins >= minMins && nowMins <= maxMins);
    } else {
        // Midnight-crossing window: min > max (e.g., 23:00 – 01:00)
        inWindow = (nowMins >= minMins || nowMins <= maxMins);
    }

    if (!inWindow) {
        return false;  // Not in wake window yet
    }

    // Force alarm at max wake time regardless of sleep stage
    bool atMax;
    if (minMins <= maxMins) {
        atMax = (nowMins >= maxMins);
    } else {
        // For midnight-crossing, we consider "at max" if we're past max
        // but before min (i.e., in the early morning portion)
        atMax = (nowMins >= maxMins && nowMins < minMins);
    }

    if (atMax) {
        Serial.println("[SleepTracker] Max wake time reached — forcing alarm!");
        return true;
    }

    // Within window: check for prolonged light sleep
    if (consecutiveLight >= globalConfig.lightSleepThresh) {
        Serial.printf("[SleepTracker] Prolonged light sleep (%d epochs) in wake window — triggering alarm!\n",
                      consecutiveLight);
        return true;
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────

SleepStage SleepTracker::getCurrentStage() const {
    if (epochCount == 0) return STAGE_UNKNOWN;
    return hypnogram[epochCount - 1].stage;
}

float SleepTracker::getCurrentConfidence() const {
    if (epochCount == 0) return 0;
    return hypnogram[epochCount - 1].confidence;
}

int SleepTracker::getConsecutiveLightCount() const {
    return consecutiveLight;
}

const SleepEpochData* SleepTracker::getHypnogram() const {
    return hypnogram;
}

int SleepTracker::getHypnogramLength() const {
    return epochCount;
}

bool SleepTracker::isActive() const {
    return epochCount > 0;
}

// ─────────────────────────────────────────────────────────────────────
// Sleep Statistics
// ─────────────────────────────────────────────────────────────────────

int SleepTracker::getStageMinutes(SleepStage stage) const {
    if (stage >= 4) return 0;
    // Each epoch = EEG_EPOCH_SECONDS (30s) → divide by 2 for minutes
    return (stageEpochs[stage] * EEG_EPOCH_SECONDS) / 60;
}

int SleepTracker::getTotalSleepMinutes() const {
    // Total non-wake time
    return getStageMinutes(STAGE_LIGHT) +
           getStageMinutes(STAGE_DEEP) +
           getStageMinutes(STAGE_REM);
}

float SleepTracker::getSleepEfficiency() const {
    if (epochCount == 0) return 0;
    int totalMinutes = (epochCount * EEG_EPOCH_SECONDS) / 60;
    if (totalMinutes == 0) return 0;
    return (float)getTotalSleepMinutes() / (float)totalMinutes;
}

int SleepTracker::getSleepOnsetMinutes() const {
    if (!sleepOnsetFound || sleepOnsetIndex < 0) return -1;
    return (sleepOnsetIndex * EEG_EPOCH_SECONDS) / 60;
}
