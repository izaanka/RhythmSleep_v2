#pragma once
// =====================================================================
// RhythmSleep — Configuration & Shared Definitions
// Target: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)
// =====================================================================

#include <Arduino.h>
#include <cmath>

// =====================================================================
// FEATURE TOGGLES — Set these to control optional hardware/features
// =====================================================================
#define ENABLE_DISPLAY      true    // Set true to enable 2.8" ST7789 TFT display
#define ENABLE_WEB_SERVER   true    // Set true to enable WiFi web dashboard

// =====================================================================
// ESP32-S3 N16R8 PIN DEFINITIONS
// Reserved GPIOs (DO NOT USE):
//   26-32: Internal flash (quad SPI)
//   33-37: Octal PSRAM (R8 variant)
//   19-20: USB D-/D+ (native USB)
//   0, 3, 45, 46: Strapping pins
// =====================================================================

// --- EEG Sensor (BioAmp EXG Pill via 2.2kΩ/1kΩ voltage divider) ---
#define PIN_EEG_ADC          1      // ADC1_CH0 — MUST use ADC1 (ADC2 conflicts with WiFi)

// --- PCF8563 RTC (I2C Bus) ---
#define PIN_RTC_SDA          42
#define PIN_RTC_SCL          41

// --- Shared SPI Bus (SPI2) — SD Card + TFT Display ---
#define PIN_SPI_MOSI         11
#define PIN_SPI_MISO         13
#define PIN_SPI_SCK          12

// --- SD Card Module (CS only — MOSI/MISO/SCK shared above) ---
#define PIN_SD_CS            10

// --- ST7789 2.8" TFT Display ---
#define PIN_TFT_CS           38
#define PIN_TFT_DC           39
#define PIN_TFT_RST          40
#define PIN_TFT_BLK          48     // Backlight PWM control

// --- DFPlayer Mini (UART1) ---
#define PIN_DFPLAYER_RX      18     // ESP32-S3 RX ← DFPlayer TX
#define PIN_DFPLAYER_TX      17     // ESP32-S3 TX → DFPlayer RX (through 1kΩ resistor)

// --- Vibration Motor (PWM via N-channel MOSFET) ---
#define PIN_VIBRATION        21

// --- Navigation Buttons (Active LOW, use internal pull-up) ---
#define PIN_BTN_UP           4
#define PIN_BTN_DOWN         5
#define PIN_BTN_SELECT       6
#define PIN_BTN_BACK         7

// =====================================================================
// EEG SIGNAL PROCESSING CONSTANTS
// =====================================================================
#define EEG_SAMPLE_RATE      256    // Hz — satisfies Nyquist for all EEG bands up to 45 Hz
#define EEG_SAMPLE_COUNT     256    // FFT window = 1 second (power-of-2 for radix-2 FFT)
#define EEG_EPOCH_SECONDS    30     // AASM standard epoch for sleep staging
#define EEG_FFTS_PER_EPOCH   (EEG_EPOCH_SECONDS)  // 30 × 1-second FFTs per epoch

// ADC settings
#define EEG_ADC_RESOLUTION   12     // 12-bit ADC (0–4095)
#define EEG_ADC_VREF         3.3f   // ESP32-S3 ADC reference voltage
#define EEG_VDIV_RATIO       0.3125f // Voltage divider: 1kΩ/(2.2kΩ+1kΩ)
#define EEG_ADC_MIDPOINT     2048   // Midpoint of 12-bit ADC

// Frequency band boundaries (Hz) — AASM standard
#define FREQ_DELTA_LOW       0.5f
#define FREQ_DELTA_HIGH      4.0f
#define FREQ_THETA_LOW       4.0f
#define FREQ_THETA_HIGH      8.0f
#define FREQ_ALPHA_LOW       8.0f
#define FREQ_ALPHA_HIGH      13.0f
#define FREQ_BETA_LOW        13.0f
#define FREQ_BETA_HIGH       30.0f
#define FREQ_GAMMA_LOW       30.0f
#define FREQ_GAMMA_HIGH      45.0f

// =====================================================================
// NEURAL NETWORK DIMENSIONS (MLP: 16→32→16→4)
// =====================================================================
#define NN_INPUT_SIZE        16     // Feature vector size
#define NN_HIDDEN1_SIZE      32     // First hidden layer neurons
#define NN_HIDDEN2_SIZE      16     // Second hidden layer neurons
#define NN_OUTPUT_SIZE       4      // Classes: WAKE, LIGHT, DEEP, REM

// Weight counts per layer (weights + biases)
#define NN_L1_WEIGHTS        (NN_INPUT_SIZE  * NN_HIDDEN1_SIZE)  // 512
#define NN_L1_BIASES         (NN_HIDDEN1_SIZE)                   // 32
#define NN_L2_WEIGHTS        (NN_HIDDEN1_SIZE * NN_HIDDEN2_SIZE) // 512
#define NN_L2_BIASES         (NN_HIDDEN2_SIZE)                   // 16
#define NN_L3_WEIGHTS        (NN_HIDDEN2_SIZE * NN_OUTPUT_SIZE)  // 64
#define NN_L3_BIASES         (NN_OUTPUT_SIZE)                    // 4
#define NN_TOTAL_PARAMS      (NN_L1_WEIGHTS + NN_L1_BIASES + \
                              NN_L2_WEIGHTS + NN_L2_BIASES + \
                              NN_L3_WEIGHTS + NN_L3_BIASES)     // 1140

// =====================================================================
// DISPLAY CONSTANTS
// =====================================================================
#define TFT_SCREEN_WIDTH     320    // Landscape orientation (rotated)
#define TFT_SCREEN_HEIGHT    240

// Color palette (RGB565)
#define COLOR_BG             0x0000  // Black
#define COLOR_TEXT            0xFFFF  // White
#define COLOR_TEXT_DIM        0x7BEF  // Gray
#define COLOR_ACCENT         0x07FF  // Cyan
#define COLOR_WAKE           0xFFE0  // Yellow
#define COLOR_LIGHT          0x07FF  // Cyan
#define COLOR_DEEP           0x001F  // Blue
#define COLOR_REM            0xF81F  // Magenta
#define COLOR_GRID           0x2945  // Dark gray
#define COLOR_WAVEFORM       0x07E0  // Green
#define COLOR_DELTA          0x001F  // Blue
#define COLOR_THETA          0x07FF  // Cyan
#define COLOR_ALPHA          0x07E0  // Green
#define COLOR_BETA           0xFFE0  // Yellow
#define COLOR_GAMMA          0xF800  // Red
#define COLOR_BAR_BG         0x18E3  // Dark gray for bar backgrounds

// =====================================================================
// ALARM CONSTANTS
// =====================================================================
#define ALARM_VOLUME_MIN     5
#define ALARM_VOLUME_MAX     28
#define ALARM_RAMP_MS        30000   // Ramp from min to max volume over 30 seconds
#define ALARM_ESCALATION_MS  120000  // Force max intensity after 2 minutes
#define VIB_PULSE_ON_MS      500     // Vibration on duration
#define VIB_PULSE_OFF_MS     500     // Vibration off duration
#define VIB_PWM_FREQ         1000    // PWM frequency for motor
#define VIB_PWM_RESOLUTION   8       // 8-bit PWM (0–255)

// =====================================================================
// BUTTON CONSTANTS
// =====================================================================
#define BTN_DEBOUNCE_MS      50
#define BTN_LONG_PRESS_MS    800
#define BTN_REPEAT_MS        200     // Auto-repeat interval for held buttons

// =====================================================================
// SD CARD FILE PATHS
// =====================================================================
#define SD_CONFIG_PATH       "/config.json"
#define SD_WEIGHTS_PATH      "/model/weights.bin"
#define SD_SLEEP_LOG_DIR     "/sleep_logs"
#define SD_RAW_EEG_DIR       "/raw_eeg"

// =====================================================================
// SLEEP TRACKING
// =====================================================================
#define MAX_HYPNOGRAM_LEN    1440    // 12 hours × 2 epochs/min × 60 min = 1440 epochs
#define DEFAULT_LIGHT_THRESH 3       // Consecutive light-sleep epochs to trigger wake-up
#define SLEEP_ONSET_THRESH   3       // Consecutive non-wake epochs to detect sleep onset

// =====================================================================
// WEB SERVER
// =====================================================================
#define WEB_SERVER_PORT      80
#define WEBSOCKET_PATH       "/ws"
#define WS_UPDATE_INTERVAL   1000    // WebSocket push interval (ms)

// =====================================================================
// ENUMERATIONS
// =====================================================================

/// Sleep stage classification output
enum SleepStage : uint8_t {
    STAGE_WAKE    = 0,
    STAGE_LIGHT   = 1,    // N1/N2 combined
    STAGE_DEEP    = 2,    // N3 / Slow-Wave Sleep
    STAGE_REM     = 3,
    STAGE_UNKNOWN = 255
};

/// TFT display screens
enum class MenuScreen : uint8_t {
    HOME = 0,             // Clock + current stage + alarm window
    WAVEFORM,             // Real-time EEG oscilloscope
    BAND_POWERS,          // Live frequency band bar chart
    SETTINGS_ALARM,       // Set min/max wake times
    SLEEP_SUMMARY,        // Previous night hypnogram + stats
    SCREEN_COUNT
};

/// Decoded button events
enum class ButtonEvent : uint8_t {
    NONE = 0,
    PRESS_UP,
    PRESS_DOWN,
    PRESS_SELECT,
    PRESS_BACK,
    LONG_SELECT,
    LONG_BACK
};

// =====================================================================
// DATA STRUCTURES
// =====================================================================

/// User configuration — persisted to SD card as JSON
struct UserConfig {
    // User profile
    char name[32]            = "User";
    uint8_t age              = 25;
    char gender[8]           = "male";    // "male", "female", "other"

    // Alarm window
    uint8_t minWakeHour      = 6;
    uint8_t minWakeMinute    = 30;
    uint8_t maxWakeHour      = 7;
    uint8_t maxWakeMinute    = 30;
    uint8_t lightSleepThresh = DEFAULT_LIGHT_THRESH;  // Epochs of light sleep to trigger alarm
    uint8_t alarmVolume      = 20;        // DFPlayer volume 0–30

    // WiFi (only used if ENABLE_WEB_SERVER)
    char wifiSSID[32]        = "RhythmSleep";
    char wifiPassword[64]    = "sleep1234";
    bool apMode              = true;      // true=AP, false=Station

    // Recording
    bool saveRawEEG          = false;     // Save raw ADC data to SD (large files!)
};

/// Computed frequency band powers for one FFT window
struct BandPowers {
    float delta   = 0;    // 0.5–4 Hz
    float theta   = 0;    // 4–8 Hz
    float alpha   = 0;    // 8–13 Hz
    float beta    = 0;    // 13–30 Hz
    float gamma   = 0;    // 30–45 Hz
    float total   = 0;    // Sum of all bands

    // Relative (percentage) powers — normalized by total
    float relDelta = 0;
    float relTheta = 0;
    float relAlpha = 0;
    float relBeta  = 0;
    float relGamma = 0;
};

/// Full feature vector extracted from one 30-second epoch
struct EEGFeatures {
    BandPowers bands;

    // Band power ratios
    float ratioDelTh    = 0;  // Delta / Theta
    float ratioThAl     = 0;  // Theta / Alpha
    float ratioThBe     = 0;  // Theta / Beta
    float ratioSlowFast = 0;  // (Delta+Theta) / (Alpha+Beta)

    // Spectral descriptors
    float spectralEdge95 = 0; // Frequency below which 95% of power lies
    float spectralEntropy = 0;

    // Hjorth time-domain parameters
    float hjorthActivity   = 0; // Signal variance
    float hjorthMobility   = 0; // sqrt(var(dy) / var(y))
    float hjorthComplexity = 0; // mobility(dy) / mobility(y)

    // Statistical
    float rmsVoltage       = 0;
    float zeroCrossingRate = 0;

    // Packed 16-element vector for neural network input
    float nnInput[NN_INPUT_SIZE] = {0};

    /// Pack selected features into the NN input vector, normalized to [0,1]
    void packForNN() {
        // Helper: clamp value to [0, 1]
        auto clamp01 = [](float v) -> float {
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        };

        // Relative band powers are already in [0, 1]
        nnInput[0]  = clamp01(bands.relDelta);
        nnInput[1]  = clamp01(bands.relTheta);
        nnInput[2]  = clamp01(bands.relAlpha);
        nnInput[3]  = clamp01(bands.relBeta);
        nnInput[4]  = clamp01(bands.relGamma);

        // Band ratios — typical range [0, ~10], normalise by dividing by max
        nnInput[5]  = clamp01(ratioDelTh / 10.0f);
        nnInput[6]  = clamp01(ratioThAl / 10.0f);
        nnInput[7]  = clamp01(ratioThBe / 10.0f);
        nnInput[8]  = clamp01(ratioSlowFast / 10.0f);

        // Spectral edge 95% — range [0.5, 45] Hz → normalise to [0, 1]
        nnInput[9]  = clamp01((spectralEdge95 - 0.5f) / 44.5f);

        // Spectral entropy — range [0, ~2.5] (max entropy of 5-bin dist = ln(5) ≈ 1.6)
        nnInput[10] = clamp01(spectralEntropy / 2.5f);

        // Hjorth Activity (variance) — range [0, ~1000], log-scale normalise
        nnInput[11] = clamp01(hjorthActivity > 0 ? (log(hjorthActivity + 1.0f) / log(1001.0f)) : 0.0f);

        // Hjorth Mobility — range [0, ~1.0]
        nnInput[12] = clamp01(hjorthMobility);

        // Hjorth Complexity — range [1, ~3], normalise to [0, 1]
        nnInput[13] = clamp01((hjorthComplexity - 1.0f) / 2.0f);

        // RMS Voltage — range [0, ~200], normalise
        nnInput[14] = clamp01(rmsVoltage / 200.0f);

        // Zero Crossing Rate — range [0, ~0.5], normalise
        nnInput[15] = clamp01(zeroCrossingRate / 0.5f);
    }
};

/// One epoch entry in the sleep log / hypnogram
struct SleepEpochData {
    uint32_t   timestamp  = 0;        // Unix timestamp
    SleepStage stage      = STAGE_UNKNOWN;
    float      confidence = 0;        // NN output confidence (0–1)
    BandPowers bands;                 // Band powers for this epoch
};

// =====================================================================
// GLOBAL SHARED STATE (defined in main.cpp)
// =====================================================================

/// SPI bus mutex — MUST be held when accessing SD card or TFT
extern SemaphoreHandle_t spiMutex;

/// Loaded user configuration
extern UserConfig globalConfig;

/// Latest processed EEG data (written by EEG task, read by display/web)
extern volatile float   g_latestFilteredSample;
extern volatile bool    g_newEpochReady;
extern EEGFeatures      g_latestFeatures;
extern SleepStage       g_currentSleepStage;
extern float            g_stageConfidences[NN_OUTPUT_SIZE];

// =====================================================================
// UTILITY FUNCTIONS
// =====================================================================

/// Convert sleep stage enum to human-readable string
inline const char* sleepStageStr(SleepStage s) {
    switch (s) {
        case STAGE_WAKE:  return "WAKE";
        case STAGE_LIGHT: return "LIGHT";
        case STAGE_DEEP:  return "DEEP";
        case STAGE_REM:   return "REM";
        default:          return "???";
    }
}

/// Get RGB565 display color for a sleep stage
inline uint16_t sleepStageColor(SleepStage s) {
    switch (s) {
        case STAGE_WAKE:  return COLOR_WAKE;
        case STAGE_LIGHT: return COLOR_LIGHT;
        case STAGE_DEEP:  return COLOR_DEEP;
        case STAGE_REM:   return COLOR_REM;
        default:          return COLOR_TEXT;
    }
}
