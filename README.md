# RhythmSleep

**Single-Channel EEG Smart Sleep Alarm & Hypnogram Tracker for ESP32-S3**

RhythmSleep is an embedded, real-time sleep monitoring and smart alarm system built for the **ESP32-S3 N16R8** microcontroller. By processing single-channel EEG signals from a BioAmp EXG Pill (or compatible biopotential sensor), RhythmSleep continuously classifies sleep stages (WAKE, LIGHT, DEEP, REM) using an on-device multi-layer perceptron (MLP) neural network. It triggers a progressive multi-modal alarm (audio + vibration) when prolonged light sleep is detected within a user-configured wake-up window.

---

## Key Features

* **Real-time EEG Signal Processing**:
  * Dual-core FreeRTOS task distribution (Core 1 dedicated to high-priority 256 Hz sampling).
  * 4th-order Butterworth bandpass filter (0.5-45 Hz) implemented via cascaded biquad (SOS) sections.
  * 256-point FFT spectral analysis computing absolute & relative band powers (Delta, Theta, Alpha, Beta, Gamma).
  * Feature extraction per 30-second epoch (band powers, band ratios, spectral edge 95%, spectral entropy, Hjorth activity/mobility/complexity, RMS voltage, zero-crossing rate).

* **On-Device Neural Network Inference**:
  * 4-layer MLP architecture (`16 Input -> 32 Dense (ReLU) -> 16 Dense (ReLU) -> 4 Softmax Output`).
  * Classifies 4 sleep stages: **WAKE (0)**, **LIGHT (1)**, **DEEP (2)**, **REM (3)**.
  * Weights loaded directly from binary file on SD card (`/model/weights.bin`).

* **Smart Sleep Alarm**:
  * Monitors sleep stages within a user-defined wake window (e.g. `06:30 - 07:30`).
  * Triggers during sustained light sleep (N1/N2) to eliminate grogginess / sleep inertia.
  * Fallback to forced alarm at the maximum wake window limit.
  * Audio volume ramping (DFPlayer Mini over UART) + MOSFET PWM vibration motor pulsing with snooze function.

* **Dual Interface**:
  * **2.8" ST7789 TFT Display**: Real-time EEG oscilloscope, frequency band bars, clock & stage status, hypnogram overview, and button-navigated settings.
  * **WiFi Web Dashboard**: Asynchronous web server (`ESPAsyncWebServer`) with WebSocket live streaming for real-time EEG waveform display, band powers, live hypnogram chart, and remote setting updates.

* **Data Logging & Storage**:
  * PCF8563 I2C RTC timekeeping with system uptime fallback.
  * CSV hypnogram session logs saved to `/sleep_logs/YYYY-MM-DD.csv`.
  * Optional raw EEG binary stream logging to `/raw_eeg/`.
  * JSON configuration persistence (`config.json`).

---

## Hardware Requirements & Pinout

### Target Microcontroller
* **ESP32-S3-DevKitC-1 (N16R8)**: 16 MB Quad Flash, 8 MB Octal PSRAM.

### Component Pin Mapping

| Component | Signal | ESP32-S3 GPIO | Notes |
| :--- | :--- | :--- | :--- |
| **EEG Sensor (BioAmp EXG)** | Output | `GPIO 1` | ADC1_CH0 (via 2.2k/1k divider) |
| **PCF8563 RTC** | SDA / SCL | `GPIO 42` / `GPIO 41` | I2C Bus |
| **Shared SPI Bus** | MOSI / MISO / SCK | `GPIO 11` / `13` / `12` | SPI2 (Shared SD & TFT) |
| **SD Card Module** | CS | `GPIO 10` | SPI Chip Select |
| **ST7789 2.8" TFT** | CS / DC / RST / BLK | `GPIO 38` / `39` / `40` / `48` | Backlight PWM on GPIO 48 |
| **DFPlayer Mini** | RX / TX | `GPIO 18` / `17` | UART1 (1k resistor on TX) |
| **Vibration Motor** | Gate | `GPIO 21` | N-Channel MOSFET PWM |
| **Navigation Buttons** | UP / DOWN / SEL / BACK | `GPIO 4` / `5` / `6` / `7` | Active LOW (Internal pull-ups) |

---

## Repository Structure

```
RhythmSleep_v2/
├── platformio.ini              # PlatformIO build configuration & dependencies
├── RhythmSleep.ino             # Arduino IDE main sketch file
├── README.md                   # Project documentation
├── .gitignore                  # Git ignore rules
├── sd_card_contents/           # SD Card file system root directory
│   ├── config.json             # System & user configuration
│   └── model/
│       └── weights.bin         # Neural network model binary weights (1140 floats)
├── src/                        # C++ Source files
│   ├── main.cpp                # PlatformIO entry point & FreeRTOS task creation
│   ├── config.h                # Global configuration, constants & data structs
│   ├── eeg_processing.h/.cpp   # ADC sampling, bandpass filter, FFT & feature extraction
│   ├── neural_network.h/.cpp   # MLP forward pass & weight loading
│   ├── sleep_tracker.h/.cpp    # Hypnogram buffer, statistics & wake decision logic
│   ├── alarm_controller.h/.cpp # DFPlayer audio ramp & vibration motor control
│   ├── button_handler.h/.cpp   # Debounced button polling & event decoding
│   ├── display_ui.h/.cpp       # ST7789 TFT UI renderer
│   ├── rtc_manager.h/.cpp      # PCF8563 RTC driver
│   ├── sd_manager.h/.cpp       # SD Card File SPI driver & CSV/JSON handling
│   └── web_server.h/.cpp       # WiFi AP/Station Async Web Server & WebSocket handler
└── tools/
    └── generate_default_weights.py # NumPy neural network weight generator script
```

---

## Flashing & Building Instructions

### Option A: Using PlatformIO (Recommended)
1. Open the project root in VS Code with PlatformIO extension installed.
2. Build & Upload:
   ```bash
   # Build firmware
   pio run

   # Upload to ESP32-S3
   pio run --target upload

   # Serial Monitor (115200 baud)
   pio device monitor
   ```

### Option B: Using Arduino IDE
1. Open `RhythmSleep.ino` in Arduino IDE.
2. Select Board: **ESP32S3 Dev Module**.
3. Board Configuration:
   * **PSRAM**: `OPI PSRAM` (or `Enabled`)
   * **Flash Size**: `16MB (128Mb)`
   * **Partition Scheme**: `16M Flash (3MB APP/9.9MB FATFS)` or `16MB Default`
4. Install Required Libraries via Library Manager (`Ctrl+Shift+I`):
   * `TFT_eSPI` (by Bodmer)
   * `RTClib` (by Adafruit)
   * `DFRobotDFPlayerMini` (by DFRobot)
   * `arduinoFFT` (by Enrique Condes / kosme)
   * `ESPAsyncWebServer` & `AsyncTCP`
   * `ArduinoJson` (by Benoit Blanchon)
5. Click **Upload**.

---

## Neural Network Weight Generation & Custom Training

### Generating Default Weights
To generate the pre-programmed weight binary (`weights.bin`) using AASM clinical heuristics and NumPy:
```bash
python3 tools/generate_default_weights.py sd_card_contents/model/weights.bin
```

### Custom Machine Learning Training (Optional)
If you collect your own sleep EEG recordings or use datasets like PhysioNet Sleep-EDF:
1. Train a 4-layer MLP (`16 -> 32 -> 16 -> 4`) using PyTorch, TensorFlow, or Scikit-learn.
2. Export the 1,140 float32 weights in row-major binary format:
   `[L1_W (512), L1_B (32), L2_W (512), L2_B (16), L3_W (64), L3_B (4)]`
3. Copy `weights.bin` to `/model/weights.bin` on your SD card.

---

## Prepare SD Card

Format a MicroSD card to **FAT32** and copy the contents of `sd_card_contents/` to the root directory:
* `/config.json`
* `/model/weights.bin`

---

## Web Dashboard & Access

When powered on, RhythmSleep starts a WiFi Access Point by default:
* **SSID**: `RhythmSleep`
* **Password**: `sleep1234`
* **Dashboard URL**: `http://192.168.4.1/`

The dashboard provides real-time WebSocket monitoring for live EEG waveforms, band powers, sleep stage confidences, hypnogram chart, and remote alarm configuration.

---

## License

Distributed under the MIT License. See `LICENSE` for more information.
