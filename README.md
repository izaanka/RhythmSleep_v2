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

## Comprehensive Tutorial: Adjusting & Training Neural Network Weights

RhythmSleep uses a 4-layer Multi-Layer Perceptron (MLP) for sleep stage classification. Weights are stored on the SD card as a contiguous binary file (`/model/weights.bin`) containing exactly **1,140 float32 values (4,560 bytes)**.

### Binary Layout Specification
Parameters are saved in row-major order:
1. `Layer 1 Weights` (shape: `32 x 16` = 512 floats)
2. `Layer 1 Biases`  (shape: `32`      = 32 floats)
3. `Layer 2 Weights` (shape: `16 x 32` = 512 floats)
4. `Layer 2 Biases`  (shape: `16`      = 16 floats)
5. `Layer 3 Weights` (shape: `4 x 16`  = 64 floats)
6. `Layer 3 Biases`  (shape: `4`       = 4 floats)

---

### Method 1: Adjusting Heuristic Weights (`generate_default_weights.py`)

If you want to fine-tune classification sensitivity without training on a large external dataset (for example, adjusting for personal baseline differences like higher baseline Alpha or lower Delta amplitude in older adults), edit `tools/generate_default_weights.py`.

#### Layer 1 Detector Structure
Layer 1 consists of 32 neurons divided into 4 detector groups (8 neurons per stage):

```python
# Neurons 0-7: WAKE Detectors
w1[0:8, 2] = 1.8   # Increase Alpha gain if wake is under-detected
w1[0:8, 3] = 1.4   # Increase Beta gain for cortical alertness
w1[0:8, 0] = -1.5  # Negative weight against Delta (anti-Deep)

# Neurons 8-15: LIGHT Sleep Detectors (N1/N2)
w1[8:16, 1] = 1.6  # Theta power gain
w1[8:16, 6] = 1.2  # Theta/Alpha ratio gain

# Neurons 16-23: DEEP Sleep Detectors (N3 / Slow-Wave)
w1[16:24, 0] = 1.8 # Delta power gain (decrease if subject has naturally lower delta)
w1[16:24, 8] = 1.4 # (Delta + Theta) / (Alpha + Beta) ratio gain

# Neurons 24-31: REM Sleep Detectors
w1[24:32, 3] = 1.6 # High Beta gain (cortical desynchronization)
w1[24:32, 10] = 1.5 # High Spectral Entropy gain (mixed frequency)
w1[24:32, 14] = -0.7 # Negative RMS gain (muscle atonia indicator)
```

After modifying the script, regenerate `weights.bin` and copy it to your SD card:
```bash
python3 tools/generate_default_weights.py sd_card_contents/model/weights.bin
```

---

### Method 2: Custom Machine Learning Training Pipeline (PyTorch)

You can train a model on real EEG recordings (e.g. PhysioNet Sleep-EDF) using PyTorch or Scikit-learn, and export the weights directly for RhythmSleep.

#### Step 1: PyTorch Model Definition
```python
import torch
import torch.nn as nn
import numpy as np

class SleepMLP(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(16, 32)
        self.relu1 = nn.ReLU()
        self.fc2 = nn.Linear(32, 16)
        self.relu2 = nn.ReLU()
        self.fc3 = nn.Linear(16, 4)

    def forward(self, x):
        x = self.relu1(self.fc1(x))
        x = self.relu2(self.fc2(x))
        x = self.fc3(x)
        return x
```

#### Step 2: Training & Feature Vector Preparation
Ensure your 16 input features are normalized to $[0, 1]$ matching ESP32 preprocessing (`config.h`):
* Band powers normalized by total power.
* Ratios divided by 10.0 and clamped to $[0, 1]$.
* Spectral Edge 95% normalized via `(SE95 - 0.5) / 44.5`.
* Spectral Entropy normalized via `Entropy / 2.5`.
* Hjorth Activity log-scaled via `log(Act + 1) / log(1001)`.

#### Step 3: Exporting Weights to `weights.bin`
```python
def export_esp32_weights(model, output_path="weights.bin"):
    w1 = model.fc1.weight.detach().cpu().numpy().astype(np.float32) # (32, 16)
    b1 = model.fc1.bias.detach().cpu().numpy().astype(np.float32)   # (32,)
    w2 = model.fc2.weight.detach().cpu().numpy().astype(np.float32) # (16, 32)
    b2 = model.fc2.bias.detach().cpu().numpy().astype(np.float32)   # (16,)
    w3 = model.fc3.weight.detach().cpu().numpy().astype(np.float32) # (4, 16)
    b3 = model.fc3.bias.detach().cpu().numpy().astype(np.float32)   # (4,)

    binary_data = np.concatenate([
        w1.flatten(), b1.flatten(),
        w2.flatten(), b2.flatten(),
        w3.flatten(), b3.flatten()
    ])
    
    assert len(binary_data) == 1140, "Weight count mismatch!"
    binary_data.tofile(output_path)
    print(f"Exported {len(binary_data)} weights ({len(binary_data)*4} bytes) to {output_path}")

# Call after training loop:
# export_esp32_weights(model, "sd_card_contents/model/weights.bin")
```

---

## Scientific & Clinical Sources for Default Heuristics

The default feature extraction rules and heuristic neural network weights in RhythmSleep are grounded in established clinical sleep medicine literature and EEG signal processing research:

1. **American Academy of Sleep Medicine (AASM) Manual for the Scoring of Sleep and Associated Events (2020)**:
   * Standard 30-second epoch windowing specification.
   * Standard frequency band definitions:
     * Delta ($\delta$): $0.5 - 4\text{ Hz}$ (Slow-Wave Sleep / N3 Deep Sleep dominance)
     * Theta ($\theta$): $4 - 8\text{ Hz}$ (N1/N2 Light Sleep & Sleep Onset marker)
     * Alpha ($\alpha$): $8 - 13\text{ Hz}$ (Wakefulness with closed eyes; posterior rhythm)
     * Beta ($\beta$): $13 - 30\text{ Hz}$ (Active cortical alertness / REM paradoxical sleep marker)
     * Gamma ($\gamma$): $30 - 45\text{ Hz}$ (Cortical processing)

2. **Rechtschaffen & Kales (R&K) Standardization (1968)**:
   * Foundational paper establishing visual and automated criteria for sleep stage differentiation.

3. **Hjorth Time-Domain Parameters (1970)**:
   * *Reference*: Hjorth, B. (1970). *"EEG analysis based on time domain properties"*. Electroencephalography and Clinical Neurophysiology, 29(3), 306-310.
   * Defines **Activity** (variance), **Mobility** (mean frequency estimate), and **Complexity** (frequency change rate), allowing rapid, low-power time-domain characterization on embedded hardware.

4. **PhysioNet Sleep-EDF Database**:
   * *Reference*: Kemp, B., Zwinderman, A. H., Tuk, B., Kamphuisen, H. A., & Oberye, J. J. (2000). *"Analysis of a sleep-dependent neuronal feedback loop: the slow-wave microcontinuity of the physiology of sleep"*. IEEE Transactions on Biomedical Engineering, 47(9), 1185-1194.
   * Gold-standard public sleep EEG database used as reference for feature normalization ranges.

5. **Spectral Edge Frequency 95% (SEF95) & Spectral Entropy**:
   * *References*:
     * Inouye, T., et al. (1991). *"Quantification of EEG irregularity by use of entropy"*. Electroencephalography and Clinical Neurophysiology, 79(3), 204-210.
     * Rampil, I. J. (1998). *"A primer for EEG signal processing in anesthesia"*. Anesthesiology, 89(4), 980-1002.
   * Used to distinguish high-entropy desynchronized cortical states (REM & WAKE) from low-entropy synchronized slow waves (N3 DEEP).

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
