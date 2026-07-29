// =====================================================================
// RhythmSleep — Main Entry Point & FreeRTOS Task Orchestration
// Target: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)
//
// This file wires together all modules and schedules them across
// the ESP32-S3's dual Xtensa LX7 cores using FreeRTOS tasks:
//
//   Core 1 (high priority):  EEG sampling + filtering at 256 Hz
//   Core 0 (mixed priority): FFT, NN inference, sleep tracking,
//                             alarm, display, web server, buttons
// =====================================================================

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "config.h"
#include "eeg_processing.h"
#include "neural_network.h"
#include "sleep_tracker.h"
#include "rtc_manager.h"
#include "sd_manager.h"
#include "alarm_controller.h"
#include "button_handler.h"

#if ENABLE_DISPLAY
#include "display_ui.h"
#endif

#if ENABLE_WEB_SERVER
#include "web_server.h"
#endif

// ─────────────────────────────────────────────────────────────────────
// Global shared state (declared extern in config.h)
// ─────────────────────────────────────────────────────────────────────

SemaphoreHandle_t spiMutex         = nullptr;
UserConfig        globalConfig;
volatile float    g_latestFilteredSample = 0;
volatile bool     g_newEpochReady        = false;
EEGFeatures       g_latestFeatures;
SleepStage        g_currentSleepStage    = STAGE_UNKNOWN;
float             g_stageConfidences[NN_OUTPUT_SIZE] = {0};

// ─────────────────────────────────────────────────────────────────────
// Module instances
// ─────────────────────────────────────────────────────────────────────

static EEGProcessor      eegProcessor;
static NeuralNetwork      neuralNet;
static SleepTracker       sleepTracker;
static RTCManager         rtcManager;
static SDManager          sdManager;
static AlarmController    alarmCtrl;
static ButtonHandler      buttonHandler;

#if ENABLE_DISPLAY
static DisplayUI          displayUI;
#endif

#if ENABLE_WEB_SERVER
static WebDashboard       webDashboard;
#endif

// ─────────────────────────────────────────────────────────────────────
// Task handles
// ─────────────────────────────────────────────────────────────────────

static TaskHandle_t eegTaskHandle       = nullptr;
static TaskHandle_t processingTaskHandle = nullptr;
static TaskHandle_t uiTaskHandle        = nullptr;

// Synchronisation: EEG task notifies processing task when FFT buffer is full
static SemaphoreHandle_t fftReadySemaphore = nullptr;

// ─────────────────────────────────────────────────────────────────────
// EEG Sampling Task — Core 1, Highest Priority
//
// Runs at 256 Hz (every ~3906 µs), reading the ADC and applying the
// bandpass filter. When 256 samples are collected, signals the
// processing task to compute FFT.
// ─────────────────────────────────────────────────────────────────────

static void eegSamplingTask(void* param) {
    const unsigned long sampleIntervalUs = 1000000UL / EEG_SAMPLE_RATE; // ~3906 µs
    unsigned long lastSampleUs = micros();

    Serial.println("[EEG Task] Started on core " + String(xPortGetCoreID()));

    for (;;) {
        unsigned long now = micros();
        if (now - lastSampleUs >= sampleIntervalUs) {
            lastSampleUs += sampleIntervalUs;

            // Collect one ADC sample, apply bandpass filter, store in FFT buffer
            eegProcessor.collectSample();

            // Update the global filtered sample for display
            g_latestFilteredSample = eegProcessor.getFilteredSample();

            // When 256 samples collected, notify the processing task
            if (eegProcessor.isFFTBufferFull()) {
                xSemaphoreGive(fftReadySemaphore);
            }
        } else {
            vTaskDelay(1); // Feed watchdog and yield to IDLE task
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// Signal Processing + Sleep Logic Task — Core 0
//
// Waits for the EEG task to fill the FFT buffer, then:
//  1. Computes FFT + band powers (every 1 second)
//  2. After 30 FFTs (1 epoch = 30s), extracts features + runs NN
//  3. Records epoch to sleep tracker
//  4. Checks wake-up condition
//  5. Logs to SD card
// ─────────────────────────────────────────────────────────────────────

static void processingTask(void* param) {
    Serial.println("[Processing Task] Started on core " + String(xPortGetCoreID()));

    for (;;) {
        // Block until EEG task signals that 256 samples are ready
        if (xSemaphoreTake(fftReadySemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            // ── Step 1: Compute FFT and accumulate band powers ──
            eegProcessor.computeFFT();
            eegProcessor.resetFFTBuffer();

            // Update global band powers for display
            g_latestFeatures.bands = eegProcessor.getLatestBandPowers();

            // ── Step 2: Check if we've completed a full epoch (30 FFTs) ──
            if (eegProcessor.getFFTCount() >= EEG_FFTS_PER_EPOCH) {
                Serial.println("[Processing] Epoch complete — extracting features...");

                // Extract the 16 features from the 30-second epoch
                EEGFeatures features;
                eegProcessor.computeEpochFeatures(features);
                features.packForNN();

                // ── Step 3: Run neural network inference ──
                SleepStage stage = STAGE_UNKNOWN;
                float confidence = 0;

                if (neuralNet.isLoaded()) {
                    stage = neuralNet.classify(features.nnInput);
                    const float* conf = neuralNet.getConfidences();
                    memcpy(g_stageConfidences, conf, sizeof(g_stageConfidences));
                    confidence = conf[stage];
                } else {
                    Serial.println("[Processing] WARN: NN weights not loaded, using UNKNOWN");
                }

                // Update global state
                g_currentSleepStage = stage;
                g_latestFeatures = features;
                g_newEpochReady = true;

                // ── Step 4: Record epoch in sleep tracker ──
                rtcManager.update();
                sleepTracker.recordEpoch(stage, confidence, features.bands,
                                         rtcManager.getUnixTime());

                // ── Step 5: Check wake-up condition ──
                if (!alarmCtrl.isAlarming() && !alarmCtrl.isSnoozed()) {
                    if (sleepTracker.shouldWakeUp(rtcManager.getHour(),
                                                   rtcManager.getMinute())) {
                        Serial.println("[Processing] *** ALARM TRIGGERED ***");
                        alarmCtrl.startAlarm();
                    }
                }

                // ── Step 6: Log to SD card ──
                sdManager.logSleepEpoch(
                    rtcManager.getDateString().c_str(),
                    rtcManager.getUnixTime(),
                    stage, confidence, features.bands
                );

                if (globalConfig.saveRawEEG) {
                    sdManager.logRawEEG(
                        rtcManager.getDateString().c_str(),
                        eegProcessor.getEpochSamples(),
                        eegProcessor.getEpochSampleCount()
                    );
                }

                // Reset epoch accumulators for the next 30 seconds
                eegProcessor.resetEpoch();

                Serial.printf("[Processing] Stage: %s (%.0f%%) | Light streak: %d\n",
                              sleepStageStr(stage), confidence * 100.0f,
                              sleepTracker.getConsecutiveLightCount());
            }
        } else {
            Serial.println("[Processing] WARN: FFT semaphore timeout — EEG task may be stalled");
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// UI + Alarm Update Task — Core 0, Lower Priority
//
// Handles display rendering, button input, alarm ramping/pulsing,
// and WebSocket updates at ~20 Hz.
// ─────────────────────────────────────────────────────────────────────

static void uiTask(void* param) {
    Serial.println("[UI Task] Started on core " + String(xPortGetCoreID()));

    unsigned long lastAlarmUpdate = 0;
    unsigned long lastDisplayUpdate = 0;
    unsigned long lastWebUpdate = 0;

    for (;;) {
        unsigned long now = millis();

        // ── Buttons (poll at ~20 Hz) ──
        ButtonEvent evt = buttonHandler.poll();
        if (evt != ButtonEvent::NONE) {
            // If alarm is ringing, any button stops it
            if (alarmCtrl.isAlarming()) {
                if (evt == ButtonEvent::LONG_SELECT) {
                    alarmCtrl.snooze(5);
                    Serial.println("[UI] Alarm snoozed for 5 minutes");
                } else {
                    alarmCtrl.stopAlarm();
                    Serial.println("[UI] Alarm dismissed");
                }
            }
            #if ENABLE_DISPLAY
            else {
                // Route button events to display UI
                displayUI.handleButton(evt);
            }
            #endif
        }

        // ── Alarm update (~50 Hz for smooth pulsing) ──
        if (now - lastAlarmUpdate >= 20) {
            lastAlarmUpdate = now;
            alarmCtrl.update();
        }

        // ── Display update (~10 Hz) ──
        #if ENABLE_DISPLAY
        if (now - lastDisplayUpdate >= 100) {
            lastDisplayUpdate = now;

            // Push latest data to display
            rtcManager.update();
            displayUI.setTime(rtcManager.getHour(), rtcManager.getMinute(),
                             rtcManager.getSecond());
            displayUI.setBandPowers(g_latestFeatures.bands);
            displayUI.setSleepStage(g_currentSleepStage,
                                    g_stageConfidences[g_currentSleepStage]);
            displayUI.setWaveformSample(g_latestFilteredSample);
            displayUI.setAlarmWindow(globalConfig.minWakeHour, globalConfig.minWakeMinute,
                                     globalConfig.maxWakeHour, globalConfig.maxWakeMinute);

            if (sleepTracker.isActive()) {
                displayUI.setHypnogram(sleepTracker.getHypnogram(),
                                        sleepTracker.getHypnogramLength());
            }

            displayUI.update();
        }
        #endif

        // ── Web server update (~1 Hz) ──
        #if ENABLE_WEB_SERVER
        if (now - lastWebUpdate >= WS_UPDATE_INTERVAL) {
            lastWebUpdate = now;

            webDashboard.setBandPowers(g_latestFeatures.bands);
            webDashboard.setCurrentStage(g_currentSleepStage, g_stageConfidences);
            webDashboard.setFilteredSample(g_latestFilteredSample);

            if (sleepTracker.isActive()) {
                webDashboard.setSleepData(sleepTracker.getHypnogram(),
                                           sleepTracker.getHypnogramLength());
            }

            webDashboard.update();
        }
        #endif

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ─────────────────────────────────────────────────────────────────────
// Configuration change callback (from web dashboard)
// ─────────────────────────────────────────────────────────────────────

static void onConfigChanged(const UserConfig& newConfig) {
    globalConfig = newConfig;
    sdManager.saveConfig(globalConfig);
    Serial.println("[Config] Configuration updated and saved to SD card");

    #if ENABLE_DISPLAY
    displayUI.setAlarmWindow(globalConfig.minWakeHour, globalConfig.minWakeMinute,
                             globalConfig.maxWakeHour, globalConfig.maxWakeMinute);
    #endif
}

// ─────────────────────────────────────────────────────────────────────
// Arduino setup()
// ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(500);  // Wait for USB CDC to initialise on S3

    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════════╗");
    Serial.println("║          RhythmSleep — EEG Smart Sleep Alarm            ║");
    Serial.println("║          ESP32-S3 N16R8 · v1.0.0                        ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println();

    // ── PSRAM check ──
    if (psramInit()) {
        Serial.printf("[PSRAM] Available: %d bytes\n", ESP.getPsramSize());
    } else {
        Serial.println("[PSRAM] WARNING: PSRAM init failed! Large buffers may not allocate.");
    }

    // ── Create SPI bus mutex ──
    spiMutex = xSemaphoreCreateMutex();
    if (!spiMutex) {
        Serial.println("[FATAL] Failed to create SPI mutex");
        while (true) delay(1000);
    }

    // ── Create FFT ready semaphore ──
    fftReadySemaphore = xSemaphoreCreateBinary();

    // ── Initialise SD card (must be first — other modules load from it) ──
    Serial.println("\n── Initialising SD Card ──");
    if (sdManager.begin()) {
        Serial.println("[SD] SD card ready");

        // Load user configuration
        if (sdManager.loadConfig(globalConfig)) {
            Serial.printf("[SD] Config loaded: %s, age=%d, alarm=%02d:%02d-%02d:%02d\n",
                          globalConfig.name, globalConfig.age,
                          globalConfig.minWakeHour, globalConfig.minWakeMinute,
                          globalConfig.maxWakeHour, globalConfig.maxWakeMinute);
        } else {
            Serial.println("[SD] No config.json found — using defaults");
            sdManager.saveConfig(globalConfig);  // Write defaults
        }
    } else {
        Serial.println("[SD] WARNING: SD card init failed! Using defaults, no logging.");
    }

    // ── Initialise RTC ──
    Serial.println("\n── Initialising RTC ──");
    if (rtcManager.begin()) {
        rtcManager.update();
        Serial.printf("[RTC] Time: %s\n", rtcManager.getDateTimeString().c_str());
    } else {
        Serial.println("[RTC] WARNING: PCF8563 not found! Time will be inaccurate.");
    }

    // ── Initialise Neural Network (load weights from SD) ──
    Serial.println("\n── Initialising Neural Network ──");
    if (neuralNet.begin(SD_WEIGHTS_PATH)) {
        Serial.printf("[NN] Loaded %d parameters from %s\n", NN_TOTAL_PARAMS, SD_WEIGHTS_PATH);
    } else {
        Serial.println("[NN] WARNING: Failed to load weights! Sleep classification disabled.");
    }

    // ── Initialise EEG processor ──
    Serial.println("\n── Initialising EEG Processor ──");
    eegProcessor.begin();
    Serial.printf("[EEG] Sample rate: %d Hz, FFT size: %d, Epoch: %ds\n",
                  EEG_SAMPLE_RATE, EEG_SAMPLE_COUNT, EEG_EPOCH_SECONDS);

    // ── Initialise Sleep Tracker ──
    sleepTracker.begin();

    // ── Initialise Alarm Controller ──
    Serial.println("\n── Initialising Alarm ──");
    alarmCtrl.begin();

    // ── Initialise Buttons ──
    buttonHandler.begin();

    // ── Initialise Display (conditional) ──
    #if ENABLE_DISPLAY
    Serial.println("\n── Initialising Display ──");
    displayUI.begin();
    displayUI.setAlarmWindow(globalConfig.minWakeHour, globalConfig.minWakeMinute,
                             globalConfig.maxWakeHour, globalConfig.maxWakeMinute);
    Serial.println("[TFT] 2.8\" ST7789 display ready");
    #else
    Serial.println("\n[TFT] Display DISABLED (ENABLE_DISPLAY = false)");
    #endif

    // ── Initialise Web Server (conditional) ──
    #if ENABLE_WEB_SERVER
    Serial.println("\n── Initialising Web Server ──");
    webDashboard.begin(globalConfig);
    webDashboard.setConfigCallback(onConfigChanged);
    Serial.printf("[WEB] Dashboard available at http://%s/\n",
                  webDashboard.getIPAddress().c_str());
    #else
    Serial.println("\n[WEB] Web server DISABLED (ENABLE_WEB_SERVER = false)");
    #endif

    // ── Launch FreeRTOS tasks ──
    Serial.println("\n── Launching Tasks ──");

    // EEG sampling on Core 1 at highest priority
    xTaskCreatePinnedToCore(
        eegSamplingTask,
        "EEG_Sample",
        4096,           // Stack size (bytes)
        nullptr,
        configMAX_PRIORITIES - 1,  // Highest priority
        &eegTaskHandle,
        1               // Core 1
    );

    // Signal processing on Core 0 at medium-high priority
    xTaskCreatePinnedToCore(
        processingTask,
        "Processing",
        8192,           // Larger stack for FFT + NN
        nullptr,
        configMAX_PRIORITIES - 2,
        &processingTaskHandle,
        0               // Core 0
    );

    // UI + alarm on Core 0 at medium priority
    xTaskCreatePinnedToCore(
        uiTask,
        "UI_Alarm",
        8192,
        nullptr,
        configMAX_PRIORITIES - 3,
        &uiTaskHandle,
        0               // Core 0
    );

    Serial.println("\n════════════════════════════════════════════════════════════");
    Serial.println("  RhythmSleep is running. Sweet dreams!");
    Serial.println("════════════════════════════════════════════════════════════\n");
}

// ─────────────────────────────────────────────────────────────────────
// Arduino loop() — unused (all work is done in FreeRTOS tasks)
// ─────────────────────────────────────────────────────────────────────

void loop() {
    // All work is handled by FreeRTOS tasks.
    // This loop runs on Core 1 at priority 1 (idle-ish).
    vTaskDelay(pdMS_TO_TICKS(1000));
}
