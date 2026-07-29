// =====================================================================
// RhythmSleep — All-In-One Single File Sketch for Arduino IDE
// Target: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)
// =====================================================================

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <RTClib.h>
#include <DFRobotDFPlayerMini.h>
#include <arduinoFFT.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// =====================================================================
// FEATURE TOGGLES
// =====================================================================
#define ENABLE_DISPLAY      true
#define ENABLE_WEB_SERVER   true

#if ENABLE_DISPLAY
#include <TFT_eSPI.h>
#endif

// =====================================================================
// PIN DEFINITIONS
// =====================================================================
#define PIN_EEG_ADC          1
#define PIN_RTC_SDA          42
#define PIN_RTC_SCL          41
#define PIN_SPI_MOSI         11
#define PIN_SPI_MISO         13
#define PIN_SPI_SCK          12
#define PIN_SD_CS            10
#define PIN_TFT_CS           38
#define PIN_TFT_DC           39
#define PIN_TFT_RST          40
#define PIN_TFT_BLK          48
#define PIN_DFPLAYER_RX      18
#define PIN_DFPLAYER_TX      17
#define PIN_VIBRATION        21
#define PIN_BTN_UP           4
#define PIN_BTN_DOWN         5
#define PIN_BTN_SELECT       6
#define PIN_BTN_BACK         7

// =====================================================================
// CONSTANTS
// =====================================================================
#define EEG_SAMPLE_RATE      256
#define EEG_SAMPLE_COUNT     256
#define EEG_EPOCH_SECONDS    30
#define EEG_FFTS_PER_EPOCH   (EEG_EPOCH_SECONDS)

#define EEG_ADC_RESOLUTION   12
#define EEG_ADC_VREF         3.3f
#define EEG_VDIV_RATIO       0.3125f
#define EEG_ADC_MIDPOINT     2048

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

#define NN_INPUT_SIZE        16
#define NN_HIDDEN1_SIZE      32
#define NN_HIDDEN2_SIZE      16
#define NN_OUTPUT_SIZE       4

#define NN_L1_WEIGHTS        (NN_INPUT_SIZE  * NN_HIDDEN1_SIZE)
#define NN_L1_BIASES         (NN_HIDDEN1_SIZE)
#define NN_L2_WEIGHTS        (NN_HIDDEN1_SIZE * NN_HIDDEN2_SIZE)
#define NN_L2_BIASES         (NN_HIDDEN2_SIZE)
#define NN_L3_WEIGHTS        (NN_HIDDEN2_SIZE * NN_OUTPUT_SIZE)
#define NN_L3_BIASES         (NN_OUTPUT_SIZE)
#define NN_TOTAL_PARAMS      (NN_L1_WEIGHTS + NN_L1_BIASES + \
                              NN_L2_WEIGHTS + NN_L2_BIASES + \
                              NN_L3_WEIGHTS + NN_L3_BIASES)

#define TFT_SCREEN_WIDTH     320
#define TFT_SCREEN_HEIGHT    240

#define COLOR_BG             0x0000
#define COLOR_TEXT            0xFFFF
#define COLOR_TEXT_DIM        0x7BEF
#define COLOR_ACCENT         0x07FF
#define COLOR_WAKE           0xFFE0
#define COLOR_LIGHT          0x07FF
#define COLOR_DEEP           0x001F
#define COLOR_REM            0xF81F
#define COLOR_GRID           0x2945
#define COLOR_WAVEFORM       0x07E0
#define COLOR_DELTA          0x001F
#define COLOR_THETA          0x07FF
#define COLOR_ALPHA          0x07E0
#define COLOR_BETA           0xFFE0
#define COLOR_GAMMA          0xF800
#define COLOR_BAR_BG         0x18E3

#define ALARM_VOLUME_MIN     5
#define ALARM_VOLUME_MAX     28
#define ALARM_RAMP_MS        30000
#define ALARM_ESCALATION_MS  120000
#define VIB_PULSE_ON_MS      500
#define VIB_PULSE_OFF_MS     500
#define VIB_PWM_FREQ         1000
#define VIB_PWM_RESOLUTION   8

#define BTN_DEBOUNCE_MS      50
#define BTN_LONG_PRESS_MS    800
#define BTN_REPEAT_MS        200

#define SD_CONFIG_PATH       "/config.json"
#define SD_WEIGHTS_PATH      "/model/weights.bin"
#define SD_SLEEP_LOG_DIR     "/sleep_logs"
#define SD_RAW_EEG_DIR       "/raw_eeg"

#define MAX_HYPNOGRAM_LEN    1440
#define DEFAULT_LIGHT_THRESH 3
#define SLEEP_ONSET_THRESH   3

#define WEB_SERVER_PORT      80
#define WEBSOCKET_PATH       "/ws"
#define WS_UPDATE_INTERVAL   1000

// =====================================================================
// ENUMS & STRUCTURES
// =====================================================================

enum SleepStage : uint8_t {
    STAGE_WAKE    = 0,
    STAGE_LIGHT   = 1,
    STAGE_DEEP    = 2,
    STAGE_REM     = 3,
    STAGE_UNKNOWN = 255
};

enum class MenuScreen : uint8_t {
    HOME = 0,
    WAVEFORM,
    BAND_POWERS,
    SETTINGS_ALARM,
    SLEEP_SUMMARY,
    SCREEN_COUNT
};

enum class ButtonEvent : uint8_t {
    NONE = 0,
    PRESS_UP,
    PRESS_DOWN,
    PRESS_SELECT,
    PRESS_BACK,
    LONG_SELECT,
    LONG_BACK
};

struct UserConfig {
    char name[32]            = "User";
    uint8_t age              = 25;
    char gender[8]           = "male";
    uint8_t minWakeHour      = 6;
    uint8_t minWakeMinute    = 30;
    uint8_t maxWakeHour      = 7;
    uint8_t maxWakeMinute    = 30;
    uint8_t lightSleepThresh = DEFAULT_LIGHT_THRESH;
    uint8_t alarmVolume      = 20;
    char wifiSSID[32]        = "RhythmSleep";
    char wifiPassword[64]    = "sleep1234";
    bool apMode              = true;
    bool saveRawEEG          = false;
};

struct BandPowers {
    float delta   = 0;
    float theta   = 0;
    float alpha   = 0;
    float beta    = 0;
    float gamma   = 0;
    float total   = 0;
    float relDelta = 0;
    float relTheta = 0;
    float relAlpha = 0;
    float relBeta  = 0;
    float relGamma = 0;
};

struct EEGFeatures {
    BandPowers bands;
    float ratioDelTh    = 0;
    float ratioThAl     = 0;
    float ratioThBe     = 0;
    float ratioSlowFast = 0;
    float spectralEdge95 = 0;
    float spectralEntropy = 0;
    float hjorthActivity   = 0;
    float hjorthMobility   = 0;
    float hjorthComplexity = 0;
    float rmsVoltage       = 0;
    float zeroCrossingRate = 0;
    float nnInput[NN_INPUT_SIZE] = {0};

    void packForNN() {
        auto clamp01 = [](float v) -> float {
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        };
        nnInput[0]  = clamp01(bands.relDelta);
        nnInput[1]  = clamp01(bands.relTheta);
        nnInput[2]  = clamp01(bands.relAlpha);
        nnInput[3]  = clamp01(bands.relBeta);
        nnInput[4]  = clamp01(bands.relGamma);
        nnInput[5]  = clamp01(ratioDelTh / 10.0f);
        nnInput[6]  = clamp01(ratioThAl / 10.0f);
        nnInput[7]  = clamp01(ratioThBe / 10.0f);
        nnInput[8]  = clamp01(ratioSlowFast / 10.0f);
        nnInput[9]  = clamp01((spectralEdge95 - 0.5f) / 44.5f);
        nnInput[10] = clamp01(spectralEntropy / 2.5f);
        nnInput[11] = clamp01(hjorthActivity > 0 ? (log(hjorthActivity + 1.0f) / log(1001.0f)) : 0.0f);
        nnInput[12] = clamp01(hjorthMobility);
        nnInput[13] = clamp01((hjorthComplexity - 1.0f) / 2.0f);
        nnInput[14] = clamp01(rmsVoltage / 200.0f);
        nnInput[15] = clamp01(zeroCrossingRate / 0.5f);
    }
};

struct SleepEpochData {
    uint32_t   timestamp  = 0;
    SleepStage stage      = STAGE_UNKNOWN;
    float      confidence = 0;
    BandPowers bands;
};

// =====================================================================
// GLOBAL STATE
// =====================================================================

SemaphoreHandle_t spiMutex         = nullptr;
UserConfig        globalConfig;
volatile float    g_latestFilteredSample = 0;
volatile bool     g_newEpochReady        = false;
EEGFeatures       g_latestFeatures;
SleepStage        g_currentSleepStage    = STAGE_UNKNOWN;
float             g_stageConfidences[NN_OUTPUT_SIZE] = {0};

inline const char* sleepStageStr(SleepStage s) {
    switch (s) {
        case STAGE_WAKE:  return "WAKE";
        case STAGE_LIGHT: return "LIGHT";
        case STAGE_DEEP:  return "DEEP";
        case STAGE_REM:   return "REM";
        default:          return "???";
    }
}

inline uint16_t sleepStageColor(SleepStage s) {
    switch (s) {
        case STAGE_WAKE:  return COLOR_WAKE;
        case STAGE_LIGHT: return COLOR_LIGHT;
        case STAGE_DEEP:  return COLOR_DEEP;
        case STAGE_REM:   return COLOR_REM;
        default:          return COLOR_TEXT;
    }
}

// =====================================================================
// RTC MANAGER
// =====================================================================

class RTCManager {
public:
    bool begin() {
        Wire.begin(PIN_RTC_SDA, PIN_RTC_SCL);
        if (!rtc.begin(&Wire)) {
            Serial.println("Couldn't find RTC");
            rtcReady = false;
            return false;
        }
        rtcReady = true;
        if (rtc.lostPower()) {
            Serial.println("RTC lost power, setting build time!");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        return true;
    }
    
    void update() {
        if (rtcReady) {
            DateTime now = rtc.now();
            year = now.year(); month = now.month(); day = now.day();
            hour = now.hour(); minute = now.minute(); second = now.second();
            unixTime = now.unixtime();
        } else {
            unsigned long sec = millis() / 1000;
            second = sec % 60; minute = (sec / 60) % 60; hour = (sec / 3600) % 24;
            unixTime = sec;
        }
    }
    
    uint8_t getHour() { return hour; }
    uint8_t getMinute() { return minute; }
    uint8_t getSecond() { return second; }
    uint32_t getUnixTime() { return unixTime; }
    
    String getTimeString() {
        char buf[10];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
        return String(buf);
    }
    
    String getDateString() {
        char buf[12];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return String(buf);
    }
    
    String getDateTimeString() {
        char buf[20];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
        return String(buf);
    }
    
private:
    uint8_t hour = 0, minute = 0, second = 0;
    uint16_t year = 2025;
    uint8_t month = 1, day = 1;
    uint32_t unixTime = 0;
    bool rtcReady = false;
    RTC_PCF8563 rtc;
};

const float DEFAULT_NN_WEIGHTS[1140] PROGMEM = {
    -1.507168f, -0.027227f, 1.782697f, 1.422662f, 0.415028f, -0.029327f, 0.002980f, 0.022542f,
    -1.250082f, 0.816301f, -0.019879f, 0.017118f, -0.022898f, -0.054146f, -0.048826f, 0.601443f,
    -1.492208f, -0.027129f, 1.969158f, 1.450154f, 0.398018f, -0.036330f, -0.019555f, 0.001422f,
    -1.225812f, 0.788463f, 0.030189f, -0.017307f, 0.025071f, -0.033891f, 0.015894f, 0.643247f,
    -1.574149f, -0.023907f, 2.117312f, 1.593909f, 0.411134f, -0.018120f, 0.002598f, -0.004670f,
    -1.164967f, 0.807633f, 0.010128f, -0.012356f, -0.014628f, -0.012977f, 0.011834f, 0.587371f,
    -1.491307f, 0.062262f, 2.276134f, 1.690219f, 0.436036f, -0.012242f, -0.061144f, -0.030243f,
    -1.256124f, 0.789455f, 0.000553f, 0.050293f, 0.009808f, -0.006573f, 0.024882f, 0.533666f,
    -1.492932f, 0.023126f, 2.355643f, 1.834313f, 0.410155f, -0.012459f, 0.018983f, 0.068121f,
    -1.194544f, 0.807447f, -0.013781f, -0.025495f, 0.024910f, -0.025683f, 0.002147f, 0.585670f,
    -1.485631f, 0.010010f, 2.581126f, 1.884699f, 0.391904f, -0.029363f, -0.013329f, 0.011319f,
    -1.177290f, 0.772335f, 0.026088f, 0.040669f, 0.012403f, 0.056304f, -0.023214f, 0.562660f,
    -1.553362f, 0.044881f, 2.719631f, 1.998333f, 0.408399f, -0.033765f, 0.073373f, 0.003877f,
    -1.196718f, 0.821773f, 0.014430f, 0.006717f, -0.023714f, 0.014144f, 0.056461f, 0.640363f,
    -1.452204f, -0.015336f, 2.820312f, 2.096226f, 0.401672f, 0.032826f, -0.050774f, 0.045887f,
    -1.204740f, 0.787194f, -0.030363f, -0.049646f, 0.024695f, 0.002200f, -0.038699f, 0.561148f,
    -0.310074f, 1.650071f, -0.807788f, -0.045094f, -0.007372f, -0.008182f, 1.119093f, 0.598371f,
    -0.006928f, 0.320886f, 0.455469f, 0.033797f, -0.008067f, -0.033196f, 0.077201f, 0.001777f,
    -0.299582f, 1.749276f, -0.794057f, -0.004331f, -0.017210f, -0.016406f, 1.199017f, 0.583697f,
    -0.008906f, 0.405524f, 0.403980f, 0.032231f, -0.018155f, 0.018599f, 0.089851f, 0.012521f,
    -0.301389f, 1.890610f, -0.835467f, 0.049755f, -0.019550f, 0.006247f, 1.189578f, 0.627798f,
    0.017326f, 0.428780f, 0.413757f, 0.055819f, -0.011680f, -0.014389f, 0.090623f, -0.008272f,
    -0.347712f, 2.062085f, -0.803730f, 0.002813f, -0.030588f, -0.021021f, 1.173821f, 0.548231f,
    -0.006093f, 0.426725f, 0.411624f, -0.017758f, -0.002130f, -0.003978f, 0.085351f, -0.004457f,
    -0.339247f, 2.215286f, -0.809160f, -0.014299f, -0.024508f, 0.023249f, 1.188358f, 0.597926f,
    -0.034509f, 0.384666f, 0.422409f, -0.022987f, -0.024599f, -0.006833f, 0.090023f, 0.000305f,
    -0.301740f, 2.366228f, -0.771960f, 0.010189f, 0.038166f, -0.015091f, 1.218512f, 0.627063f,
    -0.022204f, 0.395240f, 0.380063f, 0.021876f, 0.005116f, 0.010620f, 0.089851f, 0.005728f,
    -0.320490f, 2.474661f, -0.825838f, -0.007622f, -0.022805f, -0.018260f, 1.200388f, 0.638213f,
    0.024220f, 0.366914f, 0.403814f, -0.004060f, 0.008331f, -0.010168f, 0.089760f, 0.024479f,
    -0.309028f, 2.659972f, -0.827299f, -0.013146f, 0.040228f, -0.007663f, 1.203362f, 0.589886f,
    -0.021171f, 0.384724f, 0.427909f, -0.029806f, 0.001927f, -0.024231f, 0.063162f, 0.002396f,
    1.782057f, -0.038482f, -1.488316f, -1.218821f, 0.023242f, 1.011855f, 0.017551f, 0.012579f,
    1.393437f, -1.026402f, -0.490715f, 0.771239f, 0.008240f, -0.022849f, -0.003507f, -0.029367f,
    1.956637f, 0.003923f, -1.474775f, -1.189569f, 0.019557f, 1.018617f, -0.008138f, -0.001648f,
    1.433296f, -1.029803f, -0.496580f, 0.793392f, 0.006956f, -0.034503f, 0.002302f, 0.015798f,
    2.102636f, 0.022718f, -1.482035f, -1.185617f, -0.001402f, 0.999650f, 0.028904f, 0.002047f,
    1.378907f, -1.004457f, -0.536965f, 0.798150f, -0.040182f, -0.009411f, 0.005503f, -0.032607f,
    2.261906f, -0.004245f, -1.472714f, -1.182431f, -0.009613f, 0.978255f, -0.014264f, -0.040683f,
    1.375837f, -0.993435f, -0.499692f, 0.793540f, -0.006421f, -0.012574f, 0.013587f, -0.002061f,
    2.428236f, 0.008622f, -1.492572f, -1.229193f, -0.008985f, 0.963283f, -0.011854f, -0.021020f,
    1.432616f, -1.011843f, -0.518625f, 0.812328f, -0.023224f, 0.038480f, 0.000624f, 0.022194f,
    2.548905f, 0.020299f, -1.470511f, -1.218080f, -0.027052f, 0.971261f, 0.024976f, 0.017586f,
    1.393435f, -0.976077f, -0.513364f, 0.803875f, -0.026771f, -0.014169f, -0.037107f, -0.023363f,
    2.716167f, -0.005111f, -1.488344f, -1.206237f, 0.010214f, 0.996160f, -0.010173f, 0.005991f,
    1.399990f, -1.014674f, -0.510344f, 0.781600f, 0.021796f, -0.002164f, 0.008882f, -0.004186f,
    2.836261f, -0.017316f, -1.490899f, -1.189745f, 0.002364f, 1.013583f, -0.009772f, -0.036669f,
    1.411130f, -1.009590f, -0.523091f, 0.802163f, 0.009653f, -0.008542f, 0.003923f, -0.009943f,
    -1.405527f, 0.395726f, -0.793738f, 1.621379f, 0.773347f, -0.000632f, -0.323602f, 0.033333f,
    -0.031580f, 1.500589f, -0.771743f, 0.767137f, 0.505085f, -0.704207f, -0.722883f, 0.023028f,
    -1.418705f, 0.380721f, -0.803730f, 1.616335f, 0.798150f, -0.000609f, -0.288289f, -0.016339f,
    -0.029411f, 1.505417f, -0.805562f, 0.788544f, 0.501509f, -0.672008f, -0.669866f, -0.011666f,
    -1.424364f, 0.404555f, -0.812361f, 1.603378f, 0.767702f, 0.029815f, -0.282531f, -0.002824f,
    -0.006856f, 1.488056f, -0.808018f, 0.783637f, 0.507421f, -0.730248f, -0.730302f, -0.043516f,
    -1.393433f, 0.403756f, -0.818817f, 1.597401f, 0.814321f, -0.012580f, -0.334135f, 0.029706f,
    0.013444f, 1.470659f, -0.777977f, 0.781682f, 0.468205f, -0.709322f, -0.675034f, -0.021028f,
    -1.378036f, 0.419409f, -0.817344f, 1.594770f, 0.770954f, 0.012474f, -0.301280f, 0.007693f,
    -0.022941f, 1.502931f, -0.825227f, 0.772592f, 0.485121f, -0.701185f, -0.692484f, 0.000305f,
    -1.421679f, 0.428387f, -0.793739f, 1.608316f, 0.769931f, -0.000355f, -0.264421f, 0.015093f,
    -0.028775f, 1.528359f, -0.779707f, 0.785081f, 0.490890f, -0.686866f, -0.713597f, 0.002271f,
    -1.385750f, 0.401918f, -0.793708f, 1.591244f, 0.818817f, -0.038481f, -0.279883f, 0.012351f,
    0.004128f, 1.505705f, -0.823610f, 0.803882f, 0.505086f, -0.669865f, -0.699709f, -0.003923f,
    -1.388837f, 0.402636f, -0.810574f, 1.579450f, 0.779693f, -0.024508f, -0.323602f, 0.013587f,
    0.003080f, 1.517316f, -0.809160f, 0.816407f, 0.524911f, -0.709322f, -0.700344f, 0.015798f,
    -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f, -0.500000f,
    -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f,
    -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f, -0.600000f,
    -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f, -0.400000f,
    0.395155f, 0.468205f, 0.501509f, 0.528359f, 0.608316f, 0.673347f, 0.697401f, 0.774661f,
    -0.089886f, -0.098826f, -0.101773f, -0.114628f, -0.116406f, -0.124508f, -0.134509f, -0.145094f,
    -0.080556f, -0.096406f, -0.101444f, -0.114169f, -0.118260f, -0.123214f, -0.138699f, -0.148826f,
    -0.082584f, -0.093333f, -0.100623f, -0.111666f, -0.118821f, -0.126421f, -0.133891f, -0.149550f,
    -0.114508f, -0.122805f, -0.130189f, -0.140683f, -0.141680f, -0.150182f, -0.160128f, -0.170683f,
    0.398150f, 0.448205f, 0.498150f, 0.548205f, 0.601509f, 0.655085f, 0.701509f, 0.755085f,
    -0.081820f, -0.091021f, -0.104508f, -0.111834f, -0.118542f, -0.123224f, -0.134146f, -0.140182f,
    -0.084186f, -0.093711f, -0.103507f, -0.112849f, -0.119772f, -0.124144f, -0.136330f, -0.144881f,
    -0.080182f, -0.091648f, -0.100812f, -0.111648f, -0.119557f, -0.122987f, -0.135074f, -0.144775f,
    -0.082061f, -0.092102f, -0.101854f, -0.111422f, -0.118957f, -0.124508f, -0.135467f, -0.143765f,
    0.395155f, 0.450154f, 0.495155f, 0.550154f, 0.595155f, 0.650154f, 0.695155f, 0.750154f,
    -0.083730f, -0.098826f, -0.100609f, -0.111666f, -0.119707f, -0.124508f, -0.138837f, -0.141870f,
    -0.080556f, -0.098599f, -0.101018f, -0.110182f, -0.118985f, -0.122849f, -0.132607f, -0.142364f,
    -0.082531f, -0.094599f, -0.100344f, -0.114389f, -0.118542f, -0.123249f, -0.138481f, -0.140355f,
    -0.080707f, -0.098018f, -0.100882f, -0.112824f, -0.125683f, -0.130280f, -0.143587f, -0.140355f,
    -0.101820f, -0.113797f, -0.120721f, -0.130683f, -0.140882f, -0.150609f, -0.160144f, -0.170683f,
    -0.104128f, -0.114220f, -0.120609f, -0.130189f, -0.140707f, -0.150330f, -0.160082f, -0.170363f,
    -0.100774f, -0.112849f, -0.120305f, -0.130623f, -0.140683f, -0.150826f, -0.160155f, -0.170067f,
    0.395155f, 0.448205f, 0.501509f, 0.548205f, 0.595155f, 0.655085f, 0.701509f, 0.748205f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, 0.795155f, 0.895155f, 0.995155f, 1.095155f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, 0.795155f, 0.895155f, 0.995155f, 1.095155f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    0.795155f, 0.895155f, 0.995155f, 1.095155f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f, -0.200000f,
    -0.200000f, -0.200000f, -0.200000f, -0.200000f, 0.795155f, 0.895155f, 0.995155f, 1.095155f,
    -0.100000f, -0.100000f, -0.100000f, -0.100000f
};

// =====================================================================
// NEURAL NETWORK
// =====================================================================

class NeuralNetwork {
public:
    bool begin(const char* weightsPath) {
        bool loadedFromSD = false;
        if (weightsPath != nullptr) {
            if (spiMutex != nullptr) xSemaphoreTake(spiMutex, portMAX_DELAY);
            File file = SD.open(weightsPath, FILE_READ);
            if (file) {
                size_t expectedBytes = NN_TOTAL_PARAMS * sizeof(float);
                size_t bytesRead = file.read((uint8_t*)weights, expectedBytes);
                file.close();
                if (bytesRead == expectedBytes) loadedFromSD = true;
            }
            if (spiMutex != nullptr) xSemaphoreGive(spiMutex);
        }

        if (loadedFromSD) {
            Serial.printf("Neural Network weights loaded from SD card (%s)\n", weightsPath);
        } else {
            Serial.println("SD weights unavailable - using embedded default weights in Flash");
            memcpy_P(weights, DEFAULT_NN_WEIGHTS, sizeof(DEFAULT_NN_WEIGHTS));
        }

        int offset = 0;
        w1 = &weights[offset]; offset += NN_L1_WEIGHTS;
        b1 = &weights[offset]; offset += NN_L1_BIASES;
        w2 = &weights[offset]; offset += NN_L2_WEIGHTS;
        b2 = &weights[offset]; offset += NN_L2_BIASES;
        w3 = &weights[offset]; offset += NN_L3_WEIGHTS;
        b3 = &weights[offset]; offset += NN_L3_BIASES;

        loaded = true;
        return true;
    }

    SleepStage classify(const float* features) {
        if (!loaded) return STAGE_UNKNOWN;
        float out1[NN_HIDDEN1_SIZE], out2[NN_HIDDEN2_SIZE];
        
        matmul(features, w1, b1, out1, NN_INPUT_SIZE, NN_HIDDEN1_SIZE);
        relu(out1, NN_HIDDEN1_SIZE);
        
        matmul(out1, w2, b2, out2, NN_HIDDEN1_SIZE, NN_HIDDEN2_SIZE);
        relu(out2, NN_HIDDEN2_SIZE);
        
        matmul(out2, w3, b3, confidences, NN_HIDDEN2_SIZE, NN_OUTPUT_SIZE);
        softmax(confidences, NN_OUTPUT_SIZE);
        
        int bestClass = 0;
        float maxConf = confidences[0];
        for (int i = 1; i < NN_OUTPUT_SIZE; i++) {
            if (confidences[i] > maxConf) {
                maxConf = confidences[i];
                bestClass = i;
            }
        }
        return static_cast<SleepStage>(bestClass);
    }
    
    const float* getConfidences() { return confidences; }
    bool isLoaded() { return loaded; }

private:
    void relu(float* x, int n) {
        for (int i = 0; i < n; i++) if (x[i] < 0) x[i] = 0;
    }
    
    void softmax(float* x, int n) {
        float maxVal = x[0];
        for (int i = 1; i < n; i++) if (x[i] > maxVal) maxVal = x[i];
        float sumExp = 0.0f;
        for (int i = 0; i < n; i++) {
            x[i] = exp(x[i] - maxVal);
            sumExp += x[i];
        }
        for (int i = 0; i < n; i++) x[i] /= sumExp;
    }
    
    void matmul(const float* input, const float* w, const float* b, float* output, int inSize, int outSize) {
        for (int j = 0; j < outSize; j++) {
            float sum = b[j];
            for (int i = 0; i < inSize; i++) {
                sum += input[i] * w[j * inSize + i];
            }
            output[j] = sum;
        }
    }

    float weights[NN_TOTAL_PARAMS];
    float confidences[NN_OUTPUT_SIZE] = {0};
    bool loaded = false;
    float *w1, *b1, *w2, *b2, *w3, *b3;
};

// =====================================================================
// EEG PROCESSOR
// =====================================================================

class EEGProcessor {
public:
    void begin() {
        size_t bufferSize = EEG_FFTS_PER_EPOCH * EEG_SAMPLE_COUNT * sizeof(float);
        epochSamples = (float*)ps_malloc(bufferSize);
        epochSampleCount = 0;
        resetEpoch();
        resetFFTBuffer();
        for (int i = 0; i < TFT_SCREEN_WIDTH; i++) waveformBuf[i] = 0;
    }

    float bandpassFilter(float input) {
        float output;
        static float z1_1 = 0, z2_1 = 0;
        float x1 = input - (-1.97278f * z1_1) - (0.97298f * z2_1);
        output = 0.98639f * x1 + (-1.97278f * z1_1) + 0.98639f * z2_1;
        z2_1 = z1_1; z1_1 = x1;

        static float z1_2 = 0, z2_2 = 0;
        float x2 = output - (-1.47549f * z1_2) - (0.58691f * z2_2);
        output = 1.0f * x2 + 2.0f * z1_2 + 1.0f * z2_2;
        z2_2 = z1_2; z1_2 = x2;

        static float z1_3 = 0, z2_3 = 0;
        float x3 = output - (-1.70096f * z1_3) - (0.78170f * z2_3);
        output = 1.0f * x3 + (-2.0f * z1_3) + 1.0f * z2_3;
        z2_3 = z1_3; z1_3 = x3;

        static float z1_4 = 0, z2_4 = 0;
        float x4 = output - (-1.88437f * z1_4) - (0.89257f * z2_4);
        output = 0.02786f * x4 + 0.05573f * z1_4 + 0.02786f * z2_4;
        z2_4 = z1_4; z1_4 = x4;

        return output;
    }

    void collectSample() {
        int rawValue = analogRead(PIN_EEG_ADC);
        float centered = (float)rawValue - EEG_ADC_MIDPOINT;
        float filtered = bandpassFilter(centered);
        latestFilteredSample = filtered;

        if (sampleIndex < EEG_SAMPLE_COUNT) {
            vReal[sampleIndex] = filtered;
            vImag[sampleIndex] = 0.0;
            sampleIndex++;
        }

        if (epochSamples != nullptr && epochSampleCount < (EEG_FFTS_PER_EPOCH * EEG_SAMPLE_COUNT)) {
            epochSamples[epochSampleCount++] = filtered;
        }

        waveformBuf[waveformIdx] = filtered;
        waveformIdx = (waveformIdx + 1) % TFT_SCREEN_WIDTH;
    }

    bool isFFTBufferFull() { return sampleIndex >= EEG_SAMPLE_COUNT; }

    void computeFFT() {
        if (sampleIndex < EEG_SAMPLE_COUNT) return;
        FFT.windowing(vReal, EEG_SAMPLE_COUNT, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(vReal, vImag, EEG_SAMPLE_COUNT, FFT_FORWARD);
        FFT.complexToMagnitude(vReal, vImag, EEG_SAMPLE_COUNT);

        float delta = 0, theta = 0, alpha = 0, beta = 0, gamma = 0;
        for (int i = 1; i <= 45; i++) {
            float power = vReal[i] * vReal[i];
            if (i >= 1 && i < 4) delta += power;
            else if (i >= 4 && i < 8) theta += power;
            else if (i >= 8 && i < 13) alpha += power;
            else if (i >= 13 && i < 30) beta += power;
            else if (i >= 30 && i <= 45) gamma += power;
        }

        float total = delta + theta + alpha + beta + gamma;
        latestBands.delta = delta; latestBands.theta = theta; latestBands.alpha = alpha;
        latestBands.beta = beta; latestBands.gamma = gamma; latestBands.total = total;

        if (total > 0) {
            latestBands.relDelta = delta / total; latestBands.relTheta = theta / total;
            latestBands.relAlpha = alpha / total; latestBands.relBeta = beta / total;
            latestBands.relGamma = gamma / total;
        } else {
            latestBands.relDelta = latestBands.relTheta = latestBands.relAlpha = latestBands.relBeta = latestBands.relGamma = 0;
        }

        epochDeltaSum += delta; epochThetaSum += theta; epochAlphaSum += alpha;
        epochBetaSum += beta; epochGammaSum += gamma;
        fftCount++;
        resetFFTBuffer();
    }

    void resetFFTBuffer() { sampleIndex = 0; }
    void resetEpoch() {
        epochDeltaSum = epochThetaSum = epochAlphaSum = epochBetaSum = epochGammaSum = 0;
        fftCount = 0; epochSampleCount = 0;
    }

    void computeEpochFeatures(EEGFeatures& features) {
        if (fftCount == 0) return;
        features.bands.delta = epochDeltaSum / fftCount;
        features.bands.theta = epochThetaSum / fftCount;
        features.bands.alpha = epochAlphaSum / fftCount;
        features.bands.beta = epochBetaSum / fftCount;
        features.bands.gamma = epochGammaSum / fftCount;
        float total = features.bands.delta + features.bands.theta + features.bands.alpha + features.bands.beta + features.bands.gamma;
        features.bands.total = total;

        if (total > 0) {
            features.bands.relDelta = features.bands.delta / total;
            features.bands.relTheta = features.bands.theta / total;
            features.bands.relAlpha = features.bands.alpha / total;
            features.bands.relBeta = features.bands.beta / total;
            features.bands.relGamma = features.bands.gamma / total;
        }

        features.ratioDelTh = (features.bands.theta > 0) ? (features.bands.delta / features.bands.theta) : 0;
        features.ratioThAl = (features.bands.alpha > 0) ? (features.bands.theta / features.bands.alpha) : 0;
        features.ratioThBe = (features.bands.beta > 0) ? (features.bands.theta / features.bands.beta) : 0;
        float denom = features.bands.alpha + features.bands.beta;
        features.ratioSlowFast = (denom > 0) ? ((features.bands.delta + features.bands.theta) / denom) : 0;

        if (epochSamples != nullptr && epochSampleCount > 0) {
            float sum = 0, sumSq = 0;
            int zeroCrossings = 0;
            float prevSample = epochSamples[0];
            float dSum = 0, dSumSq = 0, ddSumSq = 0, prevD = 0;

            for (int i = 0; i < epochSampleCount; i++) {
                float s = epochSamples[i];
                sum += s; sumSq += s * s;
                if (i > 0) {
                    if ((s > 0 && prevSample < 0) || (s < 0 && prevSample > 0)) zeroCrossings++;
                    float d = s - prevSample;
                    dSum += d; dSumSq += d * d;
                    if (i > 1) {
                        float dd = d - prevD;
                        ddSumSq += dd * dd;
                    }
                    prevD = d;
                }
                prevSample = s;
            }

            float mean = sum / epochSampleCount;
            float variance = (sumSq / epochSampleCount) - (mean * mean);
            if (variance < 0.0f) variance = 0.0f;

            float dMean = dSum / (epochSampleCount - 1);
            float dVariance = (dSumSq / (epochSampleCount - 1)) - (dMean * dMean);
            if (dVariance < 0.0f) dVariance = 0.0f;

            float ddVariance = ddSumSq / (epochSampleCount - 2);
            if (ddVariance < 0.0f) ddVariance = 0.0f;

            features.hjorthActivity = variance;
            features.hjorthMobility = (variance > 0) ? sqrt(dVariance / variance) : 0;
            if (dVariance > 0 && features.hjorthMobility > 0) {
                float mobilityD = sqrt(ddVariance / dVariance);
                features.hjorthComplexity = mobilityD / features.hjorthMobility;
            } else {
                features.hjorthComplexity = 0;
            }

            features.rmsVoltage = sqrt(sumSq / epochSampleCount);
            features.zeroCrossingRate = (float)zeroCrossings / epochSampleCount;
        }

        float p[5] = {features.bands.relDelta, features.bands.relTheta, features.bands.relAlpha, features.bands.relBeta, features.bands.relGamma};
        float entropy = 0, cumulative = 0, edge95 = 0;
        bool edgeFound = false;
        float freqs[5] = {4.0f, 8.0f, 13.0f, 30.0f, 45.0f};

        for (int i = 0; i < 5; i++) {
            if (p[i] > 0) entropy -= p[i] * log(p[i]);
            cumulative += p[i];
            if (!edgeFound && cumulative >= 0.95f) {
                edge95 = freqs[i]; edgeFound = true;
            }
        }
        features.spectralEntropy = entropy;
        features.spectralEdge95 = edgeFound ? edge95 : 45.0f;
        features.packForNN();
    }

    float getFilteredSample() { return latestFilteredSample; }
    const BandPowers& getLatestBandPowers() { return latestBands; }
    int getFFTCount() { return fftCount; }
    float* getWaveformBuffer() { return waveformBuf; }
    int getWaveformLength() { return TFT_SCREEN_WIDTH; }
    const float* getEpochSamples() const { return epochSamples; }
    int getEpochSampleCount() const { return epochSampleCount; }

private:
    double vReal[EEG_SAMPLE_COUNT], vImag[EEG_SAMPLE_COUNT];
    int sampleIndex = 0;
    float epochDeltaSum = 0, epochThetaSum = 0, epochAlphaSum = 0, epochBetaSum = 0, epochGammaSum = 0;
    int fftCount = 0;
    float* epochSamples = nullptr;
    int epochSampleCount = 0;
    float waveformBuf[TFT_SCREEN_WIDTH];
    int waveformIdx = 0;
    BandPowers latestBands;
    float latestFilteredSample = 0.0f;
    ArduinoFFT<double> FFT = ArduinoFFT<double>();
};

// =====================================================================
// SLEEP TRACKER
// =====================================================================

class SleepTracker {
public:
    void begin() { reset(); }

    void reset() {
        epochCount = 0; consecutiveLight = 0;
        sleepOnsetIndex = -1; sleepOnsetFound = false;
        memset(stageEpochs, 0, sizeof(stageEpochs));
        memset(hypnogram, 0, sizeof(SleepEpochData) * MAX_HYPNOGRAM_LEN);
    }

    void recordEpoch(SleepStage stage, float confidence, const BandPowers& bands, uint32_t timestamp) {
        if (epochCount >= MAX_HYPNOGRAM_LEN) {
            memmove(&hypnogram[0], &hypnogram[1], sizeof(SleepEpochData) * (MAX_HYPNOGRAM_LEN - 1));
            epochCount = MAX_HYPNOGRAM_LEN - 1;
        }

        SleepEpochData& e = hypnogram[epochCount];
        e.timestamp = timestamp; e.stage = stage;
        e.confidence = confidence; e.bands = bands;

        if (stage < 4) stageEpochs[stage]++;
        if (stage == STAGE_LIGHT) consecutiveLight++; else consecutiveLight = 0;

        if (!sleepOnsetFound && stage != STAGE_WAKE) {
            int run = 0;
            for (int i = epochCount; i >= 0 && i > epochCount - SLEEP_ONSET_THRESH; i--) {
                if (hypnogram[i].stage != STAGE_WAKE) run++; else break;
            }
            if (run >= SLEEP_ONSET_THRESH) {
                sleepOnsetIndex = epochCount - SLEEP_ONSET_THRESH + 1;
                sleepOnsetFound = true;
            }
        }
        epochCount++;
    }

    bool shouldWakeUp(uint8_t currentHour, uint8_t currentMinute) const {
        int nowMins = currentHour * 60 + currentMinute;
        int minMins = globalConfig.minWakeHour * 60 + globalConfig.minWakeMinute;
        int maxMins = globalConfig.maxWakeHour * 60 + globalConfig.maxWakeMinute;

        bool inWindow = (minMins <= maxMins) ? (nowMins >= minMins && nowMins <= maxMins) : (nowMins >= minMins || nowMins <= maxMins);
        if (!inWindow) return false;

        bool atMax = (minMins <= maxMins) ? (nowMins >= maxMins) : (nowMins >= maxMins && nowMins < minMins);
        if (atMax) return true;
        if (consecutiveLight >= globalConfig.lightSleepThresh) return true;
        return false;
    }

    SleepStage getCurrentStage() const { return (epochCount == 0) ? STAGE_UNKNOWN : hypnogram[epochCount - 1].stage; }
    float getCurrentConfidence() const { return (epochCount == 0) ? 0 : hypnogram[epochCount - 1].confidence; }
    int getConsecutiveLightCount() const { return consecutiveLight; }
    const SleepEpochData* getHypnogram() const { return hypnogram; }
    int getHypnogramLength() const { return epochCount; }
    bool isActive() const { return epochCount > 0; }

private:
    SleepEpochData hypnogram[MAX_HYPNOGRAM_LEN];
    int epochCount = 0;
    int consecutiveLight = 0;
    int sleepOnsetIndex = -1;
    bool sleepOnsetFound = false;
    int stageEpochs[4] = {0, 0, 0, 0};
};

// =====================================================================
// ALARM CONTROLLER
// =====================================================================

static DFRobotDFPlayerMini myDFPlayer;

class AlarmController {
public:
    void begin() {
        Serial1.begin(9600, SERIAL_8N1, PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
        if (!myDFPlayer.begin(Serial1)) {
            dfPlayerReady = false;
        } else {
            dfPlayerReady = true;
            myDFPlayer.volume(currentVolume);
        }
#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
        ledcAttach(PIN_VIBRATION, VIB_PWM_FREQ, VIB_PWM_RESOLUTION);
#else
        ledcSetup(0, VIB_PWM_FREQ, VIB_PWM_RESOLUTION);
        ledcAttachPin(PIN_VIBRATION, 0);
#endif
        setVibrationDuty(0);
    }

    void update() {
        if (snoozed) {
            if (millis() >= snoozeUntilMs) { snoozed = false; startAlarm(); }
            return;
        }
        if (!alarming) return;

        unsigned long elapsed = millis() - alarmStartMs;
        if (elapsed < ALARM_ESCALATION_MS) {
            int newVolume = ALARM_VOLUME_MIN + ((ALARM_VOLUME_MAX - ALARM_VOLUME_MIN) * elapsed) / ALARM_RAMP_MS;
            if (newVolume > ALARM_VOLUME_MAX) newVolume = ALARM_VOLUME_MAX;
            if (newVolume != currentVolume) {
                currentVolume = newVolume;
                if (dfPlayerReady) myDFPlayer.volume(currentVolume);
            }

            if (vibState) {
                if (millis() - lastVibToggle >= VIB_PULSE_ON_MS) {
                    vibState = false; setVibrationDuty(0); lastVibToggle = millis();
                }
            } else {
                if (millis() - lastVibToggle >= VIB_PULSE_OFF_MS) {
                    vibState = true; setVibrationDuty(128); lastVibToggle = millis();
                }
            }
        } else {
            if (currentVolume != ALARM_VOLUME_MAX) {
                currentVolume = ALARM_VOLUME_MAX;
                if (dfPlayerReady) myDFPlayer.volume(currentVolume);
            }
            setVibrationDuty(255);
        }
    }

    void startAlarm() {
        if (alarming) return;
        alarming = true; snoozed = false;
        alarmStartMs = millis(); currentVolume = ALARM_VOLUME_MIN;
        if (dfPlayerReady) { myDFPlayer.volume(currentVolume); myDFPlayer.loop(1); }
        vibState = true; lastVibToggle = millis(); setVibrationDuty(128);
    }

    void stopAlarm() {
        alarming = false; snoozed = false;
        if (dfPlayerReady) myDFPlayer.pause();
        setVibrationDuty(0);
    }

    void snooze(int minutes = 5) {
        if (alarming) {
            alarming = false; snoozed = true;
            snoozeUntilMs = millis() + (minutes * 60000UL);
            if (dfPlayerReady) myDFPlayer.pause();
            setVibrationDuty(0);
        }
    }

    bool isAlarming() { return alarming; }
    bool isSnoozed() { return snoozed; }

private:
    void setVibrationDuty(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION_VAL) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
        ledcWrite(PIN_VIBRATION, duty);
#else
        ledcWrite(0, duty);
#endif
    }
    bool alarming = false, snoozed = false, dfPlayerReady = false;
    unsigned long alarmStartMs = 0, snoozeUntilMs = 0, lastVibToggle = 0;
    bool vibState = false;
    int currentVolume = ALARM_VOLUME_MIN;
};

// =====================================================================
// BUTTON HANDLER
// =====================================================================

class ButtonHandler {
public:
    void begin() {
        buttons[0].pin = PIN_BTN_UP; buttons[1].pin = PIN_BTN_DOWN;
        buttons[2].pin = PIN_BTN_SELECT; buttons[3].pin = PIN_BTN_BACK;
        for (int i = 0; i < 4; i++) {
            pinMode(buttons[i].pin, INPUT_PULLUP);
            buttons[i].lastState = buttons[i].currentState = true;
            buttons[i].lastDebounce = buttons[i].pressStart = 0;
            buttons[i].longFired = false;
        }
    }

    ButtonEvent poll() {
        for (int i = 0; i < 4; i++) {
            int state = checkButton(i);
            if (state == 1) {
                if (i == 0) return ButtonEvent::PRESS_UP;
                if (i == 1) return ButtonEvent::PRESS_DOWN;
                if (i == 2) return ButtonEvent::PRESS_SELECT;
                if (i == 3) return ButtonEvent::PRESS_BACK;
            } else if (state == 2) {
                if (i == 2) return ButtonEvent::LONG_SELECT;
                if (i == 3) return ButtonEvent::LONG_BACK;
            }
        }
        return ButtonEvent::NONE;
    }

private:
    struct ButtonState {
        uint8_t pin; bool lastState = true, currentState = true;
        unsigned long lastDebounce = 0, pressStart = 0; bool longFired = false;
    };
    ButtonState buttons[4];

    int checkButton(int idx) {
        bool reading = digitalRead(buttons[idx].pin);
        int result = 0;
        if (reading != buttons[idx].lastState) buttons[idx].lastDebounce = millis();

        if ((millis() - buttons[idx].lastDebounce) > BTN_DEBOUNCE_MS) {
            if (reading != buttons[idx].currentState) {
                buttons[idx].currentState = reading;
                if (buttons[idx].currentState == LOW) {
                    buttons[idx].pressStart = millis(); buttons[idx].longFired = false;
                } else {
                    if (!buttons[idx].longFired) result = 1;
                }
            }
        }
        if (buttons[idx].currentState == LOW && !buttons[idx].longFired) {
            if ((millis() - buttons[idx].pressStart) > BTN_LONG_PRESS_MS) {
                buttons[idx].longFired = true; result = 2;
            }
        }
        buttons[idx].lastState = reading;
        return result;
    }
};

// =====================================================================
// SD MANAGER
// =====================================================================

SPIClass sdSPI(FSPI);

class SDManager {
public:
    bool begin() {
        if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
            sdSPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
            if (!SD.begin(PIN_SD_CS, sdSPI)) {
                ready = false; xSemaphoreGive(spiMutex); return false;
            }
            ready = true;
            ensureDirectory(SD_SLEEP_LOG_DIR);
            ensureDirectory(SD_RAW_EEG_DIR);
            xSemaphoreGive(spiMutex); return true;
        }
        return false;
    }

    bool loadConfig(UserConfig& config) {
        if (!ready) return false;
        if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
            File file = SD.open(SD_CONFIG_PATH, FILE_READ);
            if (!file) { xSemaphoreGive(spiMutex); return false; }
            JsonDocument doc; DeserializationError error = deserializeJson(doc, file); file.close();
            if (!error) {
                if (doc.containsKey("user")) {
                    JsonObject obj = doc["user"];
                    strlcpy(config.name, obj["name"] | "User", sizeof(config.name));
                    config.age = obj["age"] | 25;
                    strlcpy(config.gender, obj["gender"] | "male", sizeof(config.gender));
                } else {
                    strlcpy(config.name, doc["name"] | "User", sizeof(config.name));
                    config.age = doc["age"] | 25;
                    strlcpy(config.gender, doc["gender"] | "male", sizeof(config.gender));
                }

                if (doc.containsKey("alarm")) {
                    JsonObject obj = doc["alarm"];
                    config.minWakeHour = obj["min_wake_hour"] | obj["minWakeHour"] | 6;
                    config.minWakeMinute = obj["min_wake_minute"] | obj["minWakeMinute"] | 30;
                    config.maxWakeHour = obj["max_wake_hour"] | obj["maxWakeHour"] | 7;
                    config.maxWakeMinute = obj["max_wake_minute"] | obj["maxWakeMinute"] | 30;
                    config.lightSleepThresh = obj["light_sleep_threshold"] | obj["lightSleepThresh"] | DEFAULT_LIGHT_THRESH;
                    config.alarmVolume = obj["alarm_volume"] | obj["alarmVolume"] | 20;
                } else {
                    config.minWakeHour = doc["minWakeHour"] | 6;
                    config.minWakeMinute = doc["minWakeMinute"] | 30;
                    config.maxWakeHour = doc["maxWakeHour"] | 7;
                    config.maxWakeMinute = doc["maxWakeMinute"] | 30;
                    config.lightSleepThresh = doc["lightSleepThresh"] | DEFAULT_LIGHT_THRESH;
                    config.alarmVolume = doc["alarmVolume"] | 20;
                }

                if (doc.containsKey("wifi")) {
                    JsonObject obj = doc["wifi"];
                    strlcpy(config.wifiSSID, obj["ssid"] | obj["wifiSSID"] | "RhythmSleep", sizeof(config.wifiSSID));
                    strlcpy(config.wifiPassword, obj["password"] | obj["wifiPassword"] | "sleep1234", sizeof(config.wifiPassword));
                    config.apMode = obj["ap_mode"] | obj["apMode"] | true;
                } else {
                    strlcpy(config.wifiSSID, doc["wifiSSID"] | "RhythmSleep", sizeof(config.wifiSSID));
                    strlcpy(config.wifiPassword, doc["wifiPassword"] | "sleep1234", sizeof(config.wifiPassword));
                    config.apMode = doc["apMode"] | true;
                }

                if (doc.containsKey("recording")) {
                    JsonObject obj = doc["recording"];
                    config.saveRawEEG = obj["save_raw_eeg"] | obj["saveRawEEG"] | false;
                } else {
                    config.saveRawEEG = doc["saveRawEEG"] | false;
                }
            }
            xSemaphoreGive(spiMutex); return error == DeserializationError::Ok;
        }
        return false;
    }

    bool saveConfig(const UserConfig& config) {
        if (!ready) return false;
        if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
            File file = SD.open(SD_CONFIG_PATH, FILE_WRITE);
            if (!file) { xSemaphoreGive(spiMutex); return false; }
            JsonDocument doc;
            doc["name"] = config.name; doc["age"] = config.age; doc["gender"] = config.gender;
            doc["minWakeHour"] = config.minWakeHour; doc["minWakeMinute"] = config.minWakeMinute;
            doc["maxWakeHour"] = config.maxWakeHour; doc["maxWakeMinute"] = config.maxWakeMinute;
            doc["lightSleepThresh"] = config.lightSleepThresh; doc["alarmVolume"] = config.alarmVolume;
            doc["wifiSSID"] = config.wifiSSID; doc["wifiPassword"] = config.wifiPassword;
            doc["apMode"] = config.apMode; doc["saveRawEEG"] = config.saveRawEEG;
            serializeJson(doc, file); file.close();
            xSemaphoreGive(spiMutex); return true;
        }
        return false;
    }

    bool logSleepEpoch(const char* dateStr, uint32_t timestamp, SleepStage stage, float confidence, const BandPowers& bands) {
        if (!ready) return false;
        String filepath = String(SD_SLEEP_LOG_DIR) + "/" + dateStr + ".csv";
        if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
            bool isNewFile = !SD.exists(filepath);
            File file = SD.open(filepath, FILE_APPEND);
            if (!file) { xSemaphoreGive(spiMutex); return false; }
            if (isNewFile) file.println("timestamp,stage,confidence,delta,theta,alpha,beta,gamma");
            file.printf("%u,%d,%.4f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                        timestamp, (int)stage, confidence, bands.delta, bands.theta, bands.alpha, bands.beta, bands.gamma);
            file.close(); xSemaphoreGive(spiMutex); return true;
        }
        return false;
    }

    bool logRawEEG(const char* dateStr, const float* samples, int count) {
        if (!ready) return false;
        String filepath = String(SD_RAW_EEG_DIR) + "/" + dateStr + "_raw.bin";
        if (xSemaphoreTake(spiMutex, portMAX_DELAY) == pdTRUE) {
            File file = SD.open(filepath, FILE_APPEND);
            if (!file) { xSemaphoreGive(spiMutex); return false; }
            file.write((const uint8_t*)samples, count * sizeof(float));
            file.close(); xSemaphoreGive(spiMutex); return true;
        }
        return false;
    }

private:
    bool ready = false;
    void ensureDirectory(const char* path) { if (!SD.exists(path)) SD.mkdir(path); }
};

// =====================================================================
// DISPLAY UI
// =====================================================================
#if ENABLE_DISPLAY

extern TFT_eSPI tft;
TFT_eSPI tft = TFT_eSPI();

class DisplayUI {
public:
    void begin() {
        pinMode(PIN_TFT_BLK, OUTPUT); digitalWrite(PIN_TFT_BLK, HIGH);
        if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
            tft.begin(); tft.setRotation(1); tft.fillScreen(COLOR_BG);
            xSemaphoreGive(spiMutex);
        }
    }

    void update() {
        unsigned long now = millis();
        if (now - lastDrawMs < 100 && currentScreen != MenuScreen::WAVEFORM) return;
        if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
            if (needsFullRedraw) { tft.fillScreen(COLOR_BG); needsFullRedraw = false; }
            switch (currentScreen) {
                case MenuScreen::HOME: drawHomeScreen(); break;
                case MenuScreen::WAVEFORM: drawWaveformScreen(); break;
                case MenuScreen::BAND_POWERS: drawBandPowerScreen(); break;
                case MenuScreen::SETTINGS_ALARM: drawSettingsScreen(); break;
                case MenuScreen::SLEEP_SUMMARY: drawSleepSummaryScreen(); break;
                default: break;
            }
            if (currentScreen != MenuScreen::WAVEFORM) lastDrawMs = now;
            xSemaphoreGive(spiMutex);
        }
    }

    void setScreen(MenuScreen screen) {
        if (currentScreen != screen) { currentScreen = screen; needsFullRedraw = true; }
    }

    void handleButton(ButtonEvent event) {
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
            needsFullRedraw = true; return;
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

    void prevScreen() {
        int prev = (int)currentScreen - 1;
        if (prev < 0) prev = (int)MenuScreen::SCREEN_COUNT - 1;
        setScreen((MenuScreen)prev);
    }

    void nextScreen() {
        int next = (int)currentScreen + 1;
        if (next >= (int)MenuScreen::SCREEN_COUNT) next = 0;
        setScreen((MenuScreen)next);
    }

    void setWaveformSample(float sample) {
        waveformBuf[waveformIdx] = sample;
        waveformIdx = (waveformIdx + 1) % TFT_SCREEN_WIDTH;
    }

    void setBandPowers(const BandPowers& bp) { displayBands = bp; }
    void setSleepStage(SleepStage stage, float confidence) { displayStage = stage; displayConfidence = confidence; }
    void setTime(uint8_t h, uint8_t m, uint8_t s) { dispHour = h; dispMin = m; dispSec = s; }
    void setAlarmWindow(uint8_t minH, uint8_t minM, uint8_t maxH, uint8_t maxM) {
        alarmMinH = minH; alarmMinM = minM; alarmMaxH = maxH; alarmMaxM = maxM;
        editMinH = minH; editMinM = minM; editMaxH = maxH; editMaxM = maxM;
    }
    void setHypnogram(const SleepEpochData* data, int count) { hypnogramData = data; hypnogramCount = count; }

private:
    void drawHeader(const char* title) {
        tft.fillRect(0, 0, TFT_SCREEN_WIDTH, 20, COLOR_GRID);
        tft.setTextColor(COLOR_TEXT); tft.setTextDatum(MC_DATUM); tft.setTextFont(2);
        tft.drawString(title, TFT_SCREEN_WIDTH / 2, 10);
    }

    void drawHomeScreen() {
        tft.setTextFont(2); tft.setTextDatum(TC_DATUM); tft.setTextColor(COLOR_ACCENT);
        tft.drawString("RhythmSleep", TFT_SCREEN_WIDTH / 2, 10);

        char timeStr[16]; sprintf(timeStr, "%02d:%02d:%02d", dispHour, dispMin, dispSec);
        tft.setTextFont(7); tft.setTextDatum(MC_DATUM); tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString(timeStr, TFT_SCREEN_WIDTH / 2, 80);

        const char* stageStr = sleepStageStr(displayStage);
        uint16_t stageCol = sleepStageColor(displayStage);
        int w = tft.textWidth(stageStr, 4) + 20;
        tft.fillRoundRect(TFT_SCREEN_WIDTH/2 - w/2, 130, w, 30, 15, stageCol);
        tft.setTextColor(COLOR_BG); tft.setTextFont(4); tft.drawString(stageStr, TFT_SCREEN_WIDTH/2, 145);

        tft.drawRect(TFT_SCREEN_WIDTH/2 - 50, 170, 100, 6, COLOR_TEXT_DIM);
        tft.fillRect(TFT_SCREEN_WIDTH/2 - 49, 171, 98, 4, COLOR_BG);
        tft.fillRect(TFT_SCREEN_WIDTH/2 - 49, 171, (int)(98.0f * displayConfidence), 4, stageCol);

        char alarmStr[32]; sprintf(alarmStr, "Wake: %02d:%02d - %02d:%02d", alarmMinH, alarmMinM, alarmMaxH, alarmMaxM);
        tft.setTextFont(2); tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG); tft.drawString(alarmStr, TFT_SCREEN_WIDTH/2, 200);
    }

    void drawWaveformScreen() {
        if (needsFullRedraw) {
            drawHeader("EEG Waveform");
            tft.drawLine(0, 130, TFT_SCREEN_WIDTH, 130, COLOR_GRID);
            int pX = 0; int pY = 130 - (int)(waveformBuf[0] * 90.0f / 500.0f);
            if (pY < 20) pY = 20; if (pY > 239) pY = 239;
            for (int x = 1; x < TFT_SCREEN_WIDTH; x++) {
                int cY = 130 - (int)(waveformBuf[x] * 90.0f / 500.0f);
                if (cY < 20) cY = 20; if (cY > 239) cY = 239;
                tft.drawLine(pX, pY, x, cY, COLOR_WAVEFORM);
                pX = x; pY = cY;
            }
        }
        static int lastX = 0, lastY = 130;
        int currentX = waveformIdx - 1; if (currentX < 0) currentX = TFT_SCREEN_WIDTH - 1;
        float val = waveformBuf[currentX];
        int y = 130 - (int)(val * 90.0f / 500.0f);
        if (y < 20) y = 20; if (y > 239) y = 239;

        int clearX = (currentX + 5) % TFT_SCREEN_WIDTH;
        tft.drawFastVLine(clearX, 20, 220, COLOR_BG);
        if (clearX % 20 == 0) tft.drawFastVLine(clearX, 20, 220, COLOR_GRID);
        tft.drawPixel(clearX, 130, COLOR_TEXT_DIM);

        if (!(currentX > 0 && currentX != lastX + 1)) tft.drawLine(lastX, lastY, currentX, y, COLOR_WAVEFORM);
        lastX = currentX; lastY = y;
    }

    void drawBandPowerScreen() {
        drawHeader("Band Powers");
        const char* labels[] = {"Delta", "Theta", "Alpha", "Beta", "Gamma"};
        float values[] = {displayBands.relDelta, displayBands.relTheta, displayBands.relAlpha, displayBands.relBeta, displayBands.relGamma};
        uint16_t colors[] = {COLOR_DELTA, COLOR_THETA, COLOR_ALPHA, COLOR_BETA, COLOR_GAMMA};
        tft.setTextFont(2); tft.setTextDatum(ML_DATUM);

        for (int i=0; i<5; i++) {
            int y = 40 + i * 35;
            tft.setTextColor(COLOR_TEXT, COLOR_BG); tft.drawString(labels[i], 10, y);
            tft.fillRect(70, y - 10, 200, 20, COLOR_BAR_BG);
            tft.fillRect(70, y - 10, (int)(200.0f * values[i]), 20, colors[i]);
            char pct[16]; sprintf(pct, "%3d%%", (int)(values[i] * 100)); tft.drawString(pct, 275, y);
        }
    }

    void drawSettingsScreen() {
        drawHeader("Alarm Settings");
        tft.setTextFont(4); tft.setTextDatum(MR_DATUM); tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString("Min Wake:", 140, 70); tft.drawString("Max Wake:", 140, 130);
        tft.setTextDatum(ML_DATUM); char buf[8];

        sprintf(buf, "%02d", editMinH);
        tft.setTextColor((settingsField == 0) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
        if (settingsField == 0 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
        tft.drawString(buf, 150, 70); tft.setTextColor(COLOR_TEXT, COLOR_BG); tft.drawString(":", 185, 70);

        sprintf(buf, "%02d", editMinM);
        tft.setTextColor((settingsField == 1) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
        if (settingsField == 1 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
        tft.drawString(buf, 200, 70);

        sprintf(buf, "%02d", editMaxH);
        tft.setTextColor((settingsField == 2) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
        if (settingsField == 2 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
        tft.drawString(buf, 150, 130); tft.setTextColor(COLOR_TEXT, COLOR_BG); tft.drawString(":", 185, 130);

        sprintf(buf, "%02d", editMaxM);
        tft.setTextColor((settingsField == 3) ? COLOR_ACCENT : COLOR_TEXT, COLOR_BG);
        if (settingsField == 3 && settingsEditing && (millis() / 500) % 2) tft.setTextColor(COLOR_BG, COLOR_BG);
        tft.drawString(buf, 200, 130);

        tft.setTextFont(2); tft.setTextDatum(MC_DATUM); tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
        tft.drawString("UP/DOWN: Adjust  SEL: Edit  BACK: Save", TFT_SCREEN_WIDTH/2, 210);
    }

    void drawSleepSummaryScreen() {
        drawHeader("Last Night");
        if (hypnogramCount == 0 || !hypnogramData) {
            tft.setTextFont(2); tft.setTextDatum(MC_DATUM); tft.setTextColor(COLOR_TEXT_DIM);
            tft.drawString("No sleep data available", TFT_SCREEN_WIDTH/2, 120); return;
        }
        int hWidth = 280, hX = 20, hY = 50, hHeight = 30;
        tft.drawRect(hX-1, hY-1, hWidth+2, hHeight+2, COLOR_GRID);
        int wakeCnt = 0, lightCnt = 0, deepCnt = 0, remCnt = 0;

        for (int i=0; i<hypnogramCount; i++) {
            int x1 = hX + (i * hWidth) / hypnogramCount;
            int x2 = hX + ((i+1) * hWidth) / hypnogramCount;
            int w = x2 - x1; if (w == 0) w = 1;
            tft.fillRect(x1, hY, w, hHeight, sleepStageColor(hypnogramData[i].stage));
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

        tft.setTextFont(2); tft.setTextDatum(ML_DATUM); char buf[64];
        sprintf(buf, "WAKE: %dm", (int)(wakeCnt * epochMins));
        tft.setTextColor(COLOR_WAKE); tft.drawString(buf, 20, 100);
        sprintf(buf, "LIGHT: %dm", (int)(lightCnt * epochMins));
        tft.setTextColor(COLOR_LIGHT); tft.drawString(buf, 20, 130);
        sprintf(buf, "DEEP: %dm", (int)(deepCnt * epochMins));
        tft.setTextColor(COLOR_DEEP); tft.drawString(buf, 160, 100);
        sprintf(buf, "REM: %dm", (int)(remCnt * epochMins));
        tft.setTextColor(COLOR_REM); tft.drawString(buf, 160, 130);
        tft.setTextColor(COLOR_TEXT);
        sprintf(buf, "Efficiency: %d%%", eff); tft.drawString(buf, 20, 170);
        sprintf(buf, "Total: %dh %dm", totalSleepMins / 60, totalSleepMins % 60); tft.drawString(buf, 160, 170);
    }

    MenuScreen currentScreen = MenuScreen::HOME;
    bool needsFullRedraw = true;
    unsigned long lastDrawMs = 0;
    float waveformBuf[TFT_SCREEN_WIDTH];
    int waveformIdx = 0;
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
#endif

// =====================================================================
// WEB SERVER
// =====================================================================
#if ENABLE_WEB_SERVER

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RhythmSleep Dashboard</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
        :root {
            --bg-color: #0d1117; --card-bg: #161b22; --text-main: #c9d1d9; --text-dim: #8b949e;
            --accent: #58a6ff; --border: #30363d; --wake: #d2a8ff; --light: #58a6ff; --deep: #1f6feb; --rem: #ff7b72;
            --delta: #1f6feb; --theta: #58a6ff; --alpha: #3fb950; --beta: #d2a8ff; --gamma: #ff7b72;
        }
        body { background-color: var(--bg-color); color: var(--text-main); font-family: 'Inter', sans-serif; margin: 0; padding: 20px; box-sizing: border-box; display: flex; flex-direction: column; align-items: center; }
        .container { width: 100%; max-width: 1200px; display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .header { width: 100%; max-width: 1200px; display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; padding-bottom: 20px; border-bottom: 1px solid var(--border); }
        .header-title { display: flex; align-items: center; gap: 10px; font-size: 24px; font-weight: 700; }
        .status-dot { width: 10px; height: 10px; border-radius: 50%; background-color: #3fb950; box-shadow: 0 0 10px #3fb950; }
        .status-dot.disconnected { background-color: #f85149; box-shadow: 0 0 10px #f85149; }
        .card { background-color: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 20px; box-shadow: 0 4px 12px rgba(0,0,0,0.2); }
        .card-title { font-size: 16px; font-weight: 600; margin: 0 0 15px 0; color: var(--text-dim); text-transform: uppercase; }
        .stage-badge { display: inline-block; padding: 8px 16px; border-radius: 20px; font-weight: 700; font-size: 24px; margin-bottom: 20px; }
        .stage-WAKE { background: rgba(210, 168, 255, 0.2); color: var(--wake); }
        .stage-LIGHT { background: rgba(88, 166, 255, 0.2); color: var(--light); }
        .stage-DEEP { background: rgba(31, 111, 235, 0.2); color: var(--deep); }
        .stage-REM { background: rgba(255, 123, 114, 0.2); color: var(--rem); }
        .conf-bars { display: flex; flex-direction: column; gap: 8px; margin-bottom: 20px; }
        .conf-row { display: flex; align-items: center; gap: 10px; font-size: 12px; }
        .conf-label { width: 50px; }
        .conf-bar-container { flex-grow: 1; height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; overflow: hidden; }
        .conf-bar-fill { height: 100%; border-radius: 3px; transition: width 0.3s ease; }
        .band-powers { display: flex; flex-direction: column; gap: 12px; }
        .band-row { display: flex; align-items: center; gap: 10px; }
        .band-label { width: 60px; font-size: 14px; font-weight: 500;}
        .band-bar-container { flex-grow: 1; height: 12px; background: rgba(255,255,255,0.1); border-radius: 6px; overflow: hidden; }
        .band-bar-fill { height: 100%; border-radius: 6px; transition: width 0.3s ease; }
        .band-pct { width: 45px; text-align: right; font-size: 12px; font-family: monospace; }
        canvas { width: 100%; background: rgba(0,0,0,0.5); border-radius: 8px; border: 1px solid var(--border); }
        #eegCanvas { height: 100px; } #hypnogramCanvas { height: 120px; margin-bottom: 15px;}
        .form-group { margin-bottom: 15px; }
        .form-group label { display: block; margin-bottom: 5px; font-size: 14px; color: var(--text-dim); }
        input[type="text"], input[type="number"], input[type="password"] { width: 100%; padding: 10px; background: rgba(255,255,255,0.05); border: 1px solid var(--border); border-radius: 6px; color: var(--text-main); box-sizing: border-box; }
        .time-inputs { display: flex; gap: 10px; align-items: center; }
        button { background: var(--accent); color: #fff; border: none; padding: 10px 20px; border-radius: 6px; font-weight: 600; cursor: pointer; width: 100%; }
        #toast { position: fixed; bottom: 20px; right: 20px; background: #3fb950; color: white; padding: 12px 24px; border-radius: 8px; transform: translateY(100px); opacity: 0; transition: all 0.3s ease; }
        #toast.show { transform: translateY(0); opacity: 1; }
        .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 14px; }
        .stat-box { background: rgba(255,255,255,0.05); padding: 10px; border-radius: 6px; text-align: center; }
        .stat-val { font-size: 18px; font-weight: 700; margin-top: 5px;}
    </style>
</head>
<body>
    <div class="header">
        <div class="header-title">RhythmSleep</div>
        <div style="display: flex; align-items: center; gap: 10px;">
            <span id="timeDisplay" style="font-family: monospace;">--:--:--</span>
            <div id="connStatus" class="status-dot disconnected"></div>
        </div>
    </div>
    <div class="container">
        <div class="card">
            <h2 class="card-title">Live Status</h2>
            <div style="text-align: center;"><div id="stageBadge" class="stage-badge stage-WAKE">WAKE</div></div>
            <div class="conf-bars">
                <div class="conf-row"><div class="conf-label">WAKE</div><div class="conf-bar-container"><div id="c-wake" class="conf-bar-fill" style="background: var(--wake); width: 0%;"></div></div></div>
                <div class="conf-row"><div class="conf-label">LIGHT</div><div class="conf-bar-container"><div id="c-light" class="conf-bar-fill" style="background: var(--light); width: 0%;"></div></div></div>
                <div class="conf-row"><div class="conf-label">DEEP</div><div class="conf-bar-container"><div id="c-deep" class="conf-bar-fill" style="background: var(--deep); width: 0%;"></div></div></div>
                <div class="conf-row"><div class="conf-label">REM</div><div class="conf-bar-container"><div id="c-rem" class="conf-bar-fill" style="background: var(--rem); width: 0%;"></div></div></div>
            </div>
            <canvas id="eegCanvas" width="600" height="200"></canvas>
        </div>
        <div class="card">
            <h2 class="card-title">Band Powers</h2>
            <div class="band-powers">
                <div class="band-row"><div class="band-label">Delta</div><div class="band-bar-container"><div id="b-delta" class="band-bar-fill" style="background: var(--delta); width: 0%;"></div></div><div id="p-delta" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Theta</div><div class="band-bar-container"><div id="b-theta" class="band-bar-fill" style="background: var(--theta); width: 0%;"></div></div><div id="p-theta" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Alpha</div><div class="band-bar-container"><div id="b-alpha" class="band-bar-fill" style="background: var(--alpha); width: 0%;"></div></div><div id="p-alpha" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Beta</div><div class="band-bar-container"><div id="b-beta" class="band-bar-fill" style="background: var(--beta); width: 0%;"></div></div><div id="p-beta" class="band-pct">0%</div></div>
                <div class="band-row"><div class="band-label">Gamma</div><div class="band-bar-container"><div id="b-gamma" class="band-bar-fill" style="background: var(--gamma); width: 0%;"></div></div><div id="p-gamma" class="band-pct">0%</div></div>
            </div>
        </div>
        <div class="card" style="grid-column: 1 / -1;">
            <h2 class="card-title">Sleep History (Hypnogram)</h2>
            <canvas id="hypnogramCanvas" width="1200" height="200"></canvas>
            <div class="stats-grid">
                <div class="stat-box">Total Sleep<div id="stat-total" class="stat-val">--h --m</div></div>
                <div class="stat-box">Efficiency<div id="stat-eff" class="stat-val">--%</div></div>
                <div class="stat-box">Deep Sleep<div id="stat-deep" class="stat-val">--h --m</div></div>
                <div class="stat-box">REM Sleep<div id="stat-rem" class="stat-val">--h --m</div></div>
            </div>
        </div>
        <div class="card" style="grid-column: 1 / -1;">
            <h2 class="card-title">Configuration</h2>
            <form id="configForm">
                <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px;">
                    <div>
                        <div class="form-group"><label>Alarm Window Start (HH:MM)</label><div class="time-inputs"><input type="number" id="cfg_minH" min="0" max="23"> : <input type="number" id="cfg_minM" min="0" max="59"></div></div>
                        <div class="form-group"><label>Alarm Window End (HH:MM)</label><div class="time-inputs"><input type="number" id="cfg_maxH" min="0" max="23"> : <input type="number" id="cfg_maxM" min="0" max="59"></div></div>
                        <div class="form-group"><label>Alarm Volume (0-30)</label><input type="number" id="cfg_vol" min="0" max="30"></div>
                    </div>
                    <div>
                        <div class="form-group"><label>WiFi SSID</label><input type="text" id="cfg_ssid"></div>
                        <div class="form-group"><label>WiFi Password</label><input type="password" id="cfg_pass"></div>
                        <div class="form-group"><label><input type="checkbox" id="cfg_ap"> Access Point Mode</label></div>
                        <div class="form-group"><label><input type="checkbox" id="cfg_raw"> Save Raw EEG to SD</label></div>
                    </div>
                </div>
                <button type="submit" style="margin-top: 15px;">Save Settings</button>
            </form>
        </div>
    </div>
    <div id="toast">Settings Saved!</div>
    <script>
        const stages = ["WAKE", "LIGHT", "DEEP", "REM"];
        const stageColors = ["#d2a8ff", "#58a6ff", "#1f6feb", "#ff7b72"];
        setInterval(() => { document.getElementById('timeDisplay').textContent = new Date().toLocaleTimeString('en-US', { hour12: false }); }, 1000);
        const eegCanvas = document.getElementById('eegCanvas'); const ctxEeg = eegCanvas.getContext('2d');
        const eegData = new Array(eegCanvas.width).fill(0); let eegIdx = 0;
        function drawEeg() {
            ctxEeg.clearRect(0, 0, eegCanvas.width, eegCanvas.height); ctxEeg.beginPath(); ctxEeg.strokeStyle = '#3fb950'; ctxEeg.lineWidth = 2;
            const mid = eegCanvas.height / 2;
            for (let i=0; i<eegCanvas.width; i++) {
                const idx = (eegIdx + i) % eegCanvas.width; const y = mid - (eegData[idx] * (eegCanvas.height/2) / 500);
                if (i===0) ctxEeg.moveTo(i, y); else ctxEeg.lineTo(i, y);
            }
            ctxEeg.stroke(); requestAnimationFrame(drawEeg);
        }
        drawEeg();
        let ws;
        function connectWs() {
            ws = new WebSocket('ws://' + window.location.hostname + '/ws');
            ws.onopen = () => { document.getElementById('connStatus').className = 'status-dot'; };
            ws.onclose = () => { document.getElementById('connStatus').className = 'status-dot disconnected'; setTimeout(connectWs, 2000); };
            ws.onmessage = (e) => {
                try {
                    const data = JSON.parse(e.data);
                    if (data.eeg !== undefined) { eegData[eegIdx] = data.eeg; eegIdx = (eegIdx + 1) % eegCanvas.width; }
                    if (data.stage !== undefined) { const s = data.stage; const sName = (s >= 0 && s <= 3) ? stages[s] : "UNKNOWN"; const badge = document.getElementById('stageBadge'); badge.textContent = sName; badge.className = 'stage-badge stage-' + sName; }
                    if (data.conf) { document.getElementById('c-wake').style.width = (data.conf[0]*100) + '%'; document.getElementById('c-light').style.width = (data.conf[1]*100) + '%'; document.getElementById('c-deep').style.width = (data.conf[2]*100) + '%'; document.getElementById('c-rem').style.width = (data.conf[3]*100) + '%'; }
                    if (data.bands) {
                        const updateBand = (id, val) => { const p = Math.min(100, Math.max(0, val * 100)); document.getElementById('b-'+id).style.width = p + '%'; document.getElementById('p-'+id).textContent = Math.round(p) + '%'; };
                        updateBand('delta', data.bands.d); updateBand('theta', data.bands.t); updateBand('alpha', data.bands.a); updateBand('beta', data.bands.b); updateBand('gamma', data.bands.g);
                    }
                } catch (e) {}
            };
        }
        connectWs();
        fetch('/api/config').then(res => res.json()).then(cfg => {
            document.getElementById('cfg_minH').value = cfg.minWakeHour; document.getElementById('cfg_minM').value = cfg.minWakeMinute;
            document.getElementById('cfg_maxH').value = cfg.maxWakeHour; document.getElementById('cfg_maxM').value = cfg.maxWakeMinute;
            document.getElementById('cfg_vol').value = cfg.alarmVolume; document.getElementById('cfg_ssid').value = cfg.wifiSSID;
            document.getElementById('cfg_pass').value = cfg.wifiPassword; document.getElementById('cfg_ap').checked = cfg.apMode;
            document.getElementById('cfg_raw').checked = cfg.saveRawEEG;
        });
        fetch('/api/sleep').then(res => res.json()).then(epochs => {
            if (!epochs || epochs.length === 0) return;
            const canvas = document.getElementById('hypnogramCanvas'); const ctx = canvas.getContext('2d'); ctx.clearRect(0,0,canvas.width,canvas.height);
            const w = canvas.width / epochs.length; let wCnt=0, lCnt=0, dCnt=0, rCnt=0;
            epochs.forEach((ep, i) => {
                const s = ep.stage;
                if (s>=0 && s<=3) { ctx.fillStyle = stageColors[s]; ctx.fillRect(i*w, 0, Math.ceil(w), canvas.height); if(s===0) wCnt++; else if(s===1) lCnt++; else if(s===2) dCnt++; else if(s===3) rCnt++; }
            });
            const epMins = 30 / 60; const tSleep = (lCnt+dCnt+rCnt)*epMins; const tTotal = (wCnt+lCnt+dCnt+rCnt)*epMins; const eff = tTotal > 0 ? (tSleep / tTotal) * 100 : 0;
            document.getElementById('stat-total').textContent = `${Math.floor(tSleep/60)}h ${Math.round(tSleep%60)}m`;
            document.getElementById('stat-eff').textContent = `${Math.round(eff)}%`;
            document.getElementById('stat-deep').textContent = `${Math.floor((dCnt*epMins)/60)}h ${Math.round((dCnt*epMins)%60)}m`;
            document.getElementById('stat-rem').textContent = `${Math.floor((rCnt*epMins)/60)}h ${Math.round((rCnt*epMins)%60)}m`;
        });
        document.getElementById('configForm').onsubmit = (e) => {
            e.preventDefault();
            const data = {
                minWakeHour: parseInt(document.getElementById('cfg_minH').value), minWakeMinute: parseInt(document.getElementById('cfg_minM').value),
                maxWakeHour: parseInt(document.getElementById('cfg_maxH').value), maxWakeMinute: parseInt(document.getElementById('cfg_maxM').value),
                alarmVolume: parseInt(document.getElementById('cfg_vol').value), wifiSSID: document.getElementById('cfg_ssid').value,
                wifiPassword: document.getElementById('cfg_pass').value, apMode: document.getElementById('cfg_ap').checked, saveRawEEG: document.getElementById('cfg_raw').checked
            };
            fetch('/api/config', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(data) }).then(res => {
                if (res.ok) { const t = document.getElementById('toast'); t.classList.add('show'); setTimeout(() => t.classList.remove('show'), 3000); }
            });
        };
    </script>
</body>
</html>
)rawliteral";

class WebDashboard {
public:
    void begin(UserConfig& config) {
        configPtr = &config;
        if (config.apMode) {
            WiFi.softAP(config.wifiSSID, config.wifiPassword);
        } else {
            WiFi.begin(config.wifiSSID, config.wifiPassword);
        }
        setupRoutes();
        ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){});
        server.addHandler(&ws);
        server.begin();
    }

    void update() {
        if (millis() - lastWsUpdate > WS_UPDATE_INTERVAL) {
            sendWebSocketUpdate();
            lastWsUpdate = millis();
        }
    }

    void setSleepData(const SleepEpochData* data, int count) { sleepData = data; sleepDataCount = count; }
    void setCurrentStage(SleepStage stage, const float* confs) {
        currentStage = stage;
        if (confs) for (int i=0; i<NN_OUTPUT_SIZE; i++) confidences[i] = confs[i];
    }
    void setBandPowers(const BandPowers& bp) { bands = bp; }
    void setFilteredSample(float sample) { latestSample = sample; }
    void setConfigCallback(std::function<void(const UserConfig&)> cb) { onConfigChange = cb; }
    String getIPAddress() {
        if (configPtr && configPtr->apMode) return WiFi.softAPIP().toString();
        return WiFi.localIP().toString();
    }

private:
    void setupRoutes() {
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ request->send_P(200, "text/html", DASHBOARD_HTML); });
        server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request){
            JsonDocument doc; doc["stage"] = (int)currentStage;
            JsonArray conf = doc["conf"].to<JsonArray>();
            for (int i=0; i<NN_OUTPUT_SIZE; i++) conf.add(confidences[i]);
            JsonObject b = doc["bands"].to<JsonObject>();
            b["d"] = bands.relDelta; b["t"] = bands.relTheta; b["a"] = bands.relAlpha; b["b"] = bands.relBeta; b["g"] = bands.relGamma;
            String res; serializeJson(doc, res); request->send(200, "application/json", res);
        });
        server.on("/api/sleep", HTTP_GET, [this](AsyncWebServerRequest *request){
            if (!sleepData || sleepDataCount == 0) { request->send(200, "application/json", "[]"); return; }
            JsonDocument doc; JsonArray arr = doc.to<JsonArray>();
            int limit = (sleepDataCount > 200) ? 200 : sleepDataCount;
            for (int i=0; i<limit; i++) {
                JsonObject obj = arr.add<JsonObject>();
                obj["stage"] = (int)sleepData[i].stage; obj["confidence"] = sleepData[i].confidence;
            }
            String res; serializeJson(doc, res); request->send(200, "application/json", res);
        });
        server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request){
            if (!configPtr) { request->send(500); return; }
            JsonDocument doc;
            doc["minWakeHour"] = configPtr->minWakeHour; doc["minWakeMinute"] = configPtr->minWakeMinute;
            doc["maxWakeHour"] = configPtr->maxWakeHour; doc["maxWakeMinute"] = configPtr->maxWakeMinute;
            doc["alarmVolume"] = configPtr->alarmVolume; doc["wifiSSID"] = configPtr->wifiSSID;
            doc["wifiPassword"] = configPtr->wifiPassword; doc["apMode"] = configPtr->apMode; doc["saveRawEEG"] = configPtr->saveRawEEG;
            String res; serializeJson(doc, res); request->send(200, "application/json", res);
        });
        server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
            this->handleConfigPost(request, data, len);
        });
    }

    void handleConfigPost(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
        if (!configPtr) { request->send(500); return; }
        JsonDocument doc; DeserializationError err = deserializeJson(doc, data, len);
        if (err) { request->send(400, "text/plain", "Invalid JSON"); return; }
        if (doc.containsKey("minWakeHour")) configPtr->minWakeHour = doc["minWakeHour"];
        if (doc.containsKey("minWakeMinute")) configPtr->minWakeMinute = doc["minWakeMinute"];
        if (doc.containsKey("maxWakeHour")) configPtr->maxWakeHour = doc["maxWakeHour"];
        if (doc.containsKey("maxWakeMinute")) configPtr->maxWakeMinute = doc["maxWakeMinute"];
        if (doc.containsKey("alarmVolume")) configPtr->alarmVolume = doc["alarmVolume"];
        if (doc.containsKey("wifiSSID")) strlcpy(configPtr->wifiSSID, doc["wifiSSID"] | "RhythmSleep", sizeof(configPtr->wifiSSID));
        if (doc.containsKey("wifiPassword")) strlcpy(configPtr->wifiPassword, doc["wifiPassword"] | "sleep1234", sizeof(configPtr->wifiPassword));
        if (doc.containsKey("apMode")) configPtr->apMode = doc["apMode"];
        if (doc.containsKey("saveRawEEG")) configPtr->saveRawEEG = doc["saveRawEEG"];
        if (onConfigChange) onConfigChange(*configPtr);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }

    void sendWebSocketUpdate() {
        if (ws.count() == 0) return;
        JsonDocument doc; doc["stage"] = (int)currentStage;
        JsonArray conf = doc["conf"].to<JsonArray>();
        for (int i=0; i<NN_OUTPUT_SIZE; i++) conf.add(confidences[i]);
        JsonObject b = doc["bands"].to<JsonObject>();
        b["d"] = bands.relDelta; b["t"] = bands.relTheta; b["a"] = bands.relAlpha; b["b"] = bands.relBeta; b["g"] = bands.relGamma;
        doc["eeg"] = latestSample;
        String res; serializeJson(doc, res); ws.textAll(res);
    }

    UserConfig* configPtr = nullptr;
    std::function<void(const UserConfig&)> onConfigChange = nullptr;
    SleepStage currentStage = STAGE_UNKNOWN;
    float confidences[NN_OUTPUT_SIZE] = {0};
    BandPowers bands;
    float latestSample = 0;
    const SleepEpochData* sleepData = nullptr;
    int sleepDataCount = 0;
    unsigned long lastWsUpdate = 0;
    AsyncWebServer server{WEB_SERVER_PORT};
    AsyncWebSocket ws{WEBSOCKET_PATH};
};
#endif

// =====================================================================
// MAIN TASK ORCHESTRATION
// =====================================================================

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

static TaskHandle_t eegTaskHandle       = nullptr;
static TaskHandle_t processingTaskHandle = nullptr;
static TaskHandle_t uiTaskHandle        = nullptr;
static SemaphoreHandle_t fftReadySemaphore = nullptr;

static void eegSamplingTask(void* param) {
    const unsigned long sampleIntervalUs = 1000000UL / EEG_SAMPLE_RATE;
    unsigned long lastSampleUs = micros();
    for (;;) {
        unsigned long now = micros();
        if (now - lastSampleUs >= sampleIntervalUs) {
            lastSampleUs += sampleIntervalUs;
            eegProcessor.collectSample();
            g_latestFilteredSample = eegProcessor.getFilteredSample();
            if (eegProcessor.isFFTBufferFull()) {
                xSemaphoreGive(fftReadySemaphore);
            }
        }
        taskYIELD();
    }
}

static void processingTask(void* param) {
    for (;;) {
        if (xSemaphoreTake(fftReadySemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            eegProcessor.computeFFT();
            eegProcessor.resetFFTBuffer();
            g_latestFeatures.bands = eegProcessor.getLatestBandPowers();

            if (eegProcessor.getFFTCount() >= EEG_FFTS_PER_EPOCH) {
                EEGFeatures features;
                eegProcessor.computeEpochFeatures(features);
                features.packForNN();

                SleepStage stage = STAGE_UNKNOWN;
                float confidence = 0;

                if (neuralNet.isLoaded()) {
                    stage = neuralNet.classify(features.nnInput);
                    const float* conf = neuralNet.getConfidences();
                    memcpy(g_stageConfidences, conf, sizeof(g_stageConfidences));
                    confidence = conf[stage];
                }

                g_currentSleepStage = stage;
                g_latestFeatures = features;
                g_newEpochReady = true;

                rtcManager.update();
                sleepTracker.recordEpoch(stage, confidence, features.bands, rtcManager.getUnixTime());

                if (!alarmCtrl.isAlarming() && !alarmCtrl.isSnoozed()) {
                    if (sleepTracker.shouldWakeUp(rtcManager.getHour(), rtcManager.getMinute())) {
                        alarmCtrl.startAlarm();
                    }
                }

                sdManager.logSleepEpoch(rtcManager.getDateString().c_str(), rtcManager.getUnixTime(), stage, confidence, features.bands);
                if (globalConfig.saveRawEEG) {
                    sdManager.logRawEEG(rtcManager.getDateString().c_str(), eegProcessor.getEpochSamples(), eegProcessor.getEpochSampleCount());
                }
                eegProcessor.resetEpoch();
            }
        }
    }
}

static void uiTask(void* param) {
    unsigned long lastAlarmUpdate = 0, lastDisplayUpdate = 0, lastWebUpdate = 0;
    for (;;) {
        unsigned long now = millis();
        ButtonEvent evt = buttonHandler.poll();
        if (evt != ButtonEvent::NONE) {
            if (alarmCtrl.isAlarming()) {
                if (evt == ButtonEvent::LONG_SELECT) alarmCtrl.snooze(5);
                else alarmCtrl.stopAlarm();
            }
#if ENABLE_DISPLAY
            else displayUI.handleButton(evt);
#endif
        }

        if (now - lastAlarmUpdate >= 20) {
            lastAlarmUpdate = now; alarmCtrl.update();
        }

#if ENABLE_DISPLAY
        if (now - lastDisplayUpdate >= 100) {
            lastDisplayUpdate = now;
            rtcManager.update();
            displayUI.setTime(rtcManager.getHour(), rtcManager.getMinute(), rtcManager.getSecond());
            displayUI.setBandPowers(g_latestFeatures.bands);
            displayUI.setSleepStage(g_currentSleepStage, g_stageConfidences[g_currentSleepStage]);
            displayUI.setWaveformSample(g_latestFilteredSample);
            displayUI.setAlarmWindow(globalConfig.minWakeHour, globalConfig.minWakeMinute, globalConfig.maxWakeHour, globalConfig.maxWakeMinute);
            if (sleepTracker.isActive()) {
                displayUI.setHypnogram(sleepTracker.getHypnogram(), sleepTracker.getHypnogramLength());
            }
            displayUI.update();
        }
#endif

#if ENABLE_WEB_SERVER
        if (now - lastWebUpdate >= WS_UPDATE_INTERVAL) {
            lastWebUpdate = now;
            webDashboard.setBandPowers(g_latestFeatures.bands);
            webDashboard.setCurrentStage(g_currentSleepStage, g_stageConfidences);
            webDashboard.setFilteredSample(g_latestFilteredSample);
            if (sleepTracker.isActive()) {
                webDashboard.setSleepData(sleepTracker.getHypnogram(), sleepTracker.getHypnogramLength());
            }
            webDashboard.update();
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void onConfigChanged(const UserConfig& newConfig) {
    globalConfig = newConfig;
    sdManager.saveConfig(globalConfig);
#if ENABLE_DISPLAY
    displayUI.setAlarmWindow(globalConfig.minWakeHour, globalConfig.minWakeMinute, globalConfig.maxWakeHour, globalConfig.maxWakeMinute);
#endif
}

void setup() {
    Serial.begin(115200);
    delay(500);

    if (psramInit()) Serial.printf("[PSRAM] Available: %d bytes\n", ESP.getPsramSize());

    spiMutex = xSemaphoreCreateMutex();
    fftReadySemaphore = xSemaphoreCreateBinary();

    if (sdManager.begin()) {
        if (!sdManager.loadConfig(globalConfig)) sdManager.saveConfig(globalConfig);
    }

    if (rtcManager.begin()) rtcManager.update();
    neuralNet.begin(SD_WEIGHTS_PATH);
    eegProcessor.begin();
    sleepTracker.begin();
    alarmCtrl.begin();
    buttonHandler.begin();

#if ENABLE_DISPLAY
    displayUI.begin();
    displayUI.setAlarmWindow(globalConfig.minWakeHour, globalConfig.minWakeMinute, globalConfig.maxWakeHour, globalConfig.maxWakeMinute);
#endif

#if ENABLE_WEB_SERVER
    webDashboard.begin(globalConfig);
    webDashboard.setConfigCallback(onConfigChanged);
#endif

    xTaskCreatePinnedToCore(eegSamplingTask, "EEG_Sample", 4096, nullptr, configMAX_PRIORITIES - 1, &eegTaskHandle, 1);
    xTaskCreatePinnedToCore(processingTask, "Processing", 8192, nullptr, configMAX_PRIORITIES - 2, &processingTaskHandle, 0);
    xTaskCreatePinnedToCore(uiTask, "UI_Alarm", 8192, nullptr, configMAX_PRIORITIES - 3, &uiTaskHandle, 0);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
