#pragma once
// =====================================================================
// RhythmSleep — Sleep Stage Tracker & Wake-Up Decision Engine
// =====================================================================

#include "config.h"

class SleepTracker {
public:
    void begin();

    /// Process one classified epoch — call every 30 seconds
    void recordEpoch(SleepStage stage, float confidence, const BandPowers& bands,
                     uint32_t timestamp);

    /// Check if the alarm should fire now
    bool shouldWakeUp(uint8_t currentHour, uint8_t currentMinute) const;

    /// Current (most recent) sleep stage
    SleepStage getCurrentStage() const;
    float      getCurrentConfidence() const;

    /// How many consecutive light-sleep epochs we've seen
    int getConsecutiveLightCount() const;

    /// Hypnogram data
    const SleepEpochData* getHypnogram() const;
    int   getHypnogramLength() const;

    /// Sleep statistics (all in minutes)
    int getTotalSleepMinutes() const;         // Non-wake time
    int getStageMinutes(SleepStage stage) const;
    float getSleepEfficiency() const;         // sleep / total time (0–1)
    int getSleepOnsetMinutes() const;         // Time from first epoch to first sustained sleep

    /// Reset for a new night
    void reset();

    /// Has sleep tracking started? (at least one epoch recorded)
    bool isActive() const;

private:
    SleepEpochData hypnogram[MAX_HYPNOGRAM_LEN];
    int epochCount = 0;

    int consecutiveLight = 0;
    int sleepOnsetIndex  = -1;       // Index of first sustained sleep epoch
    bool sleepOnsetFound = false;

    // Stage counters (in epochs)
    int stageEpochs[4] = {0, 0, 0, 0};  // WAKE, LIGHT, DEEP, REM
};
