#pragma once

#include "config.h"

class EEGProcessor {
public:
    void begin();                           // Initialize ADC, allocate buffers
    void collectSample();                   // Read ADC, apply bandpass filter, store in FFT buffer
    bool isFFTBufferFull();                 // True when 256 samples collected
    void computeFFT();                      // Perform FFT on current buffer, accumulate band powers
    void resetFFTBuffer();                  // Clear buffer for next window
    void computeEpochFeatures(EEGFeatures& features);  // Average 30 FFTs, compute all 16 NN features
    void resetEpoch();                      // Reset epoch accumulators
    
    float getFilteredSample();              // Latest bandpass-filtered sample (for display)
    const BandPowers& getLatestBandPowers();// Latest FFT band powers (for display)
    int getFFTCount();                      // How many FFTs completed in current epoch
    float* getWaveformBuffer();             // Circular buffer of recent filtered samples for display
    int getWaveformLength();                // Length of waveform buffer
    const float* getEpochSamples() const;   // Pointer to raw epoch samples buffer
    int getEpochSampleCount() const;        // Number of samples in current epoch buffer
    
private:
    // 4th-order Butterworth bandpass filter (0.5-45 Hz at 256 Hz)
    // Implemented as cascade of 4 biquad (SOS) sections
    float bandpassFilter(float input);
    
    // Internal state
    double vReal[EEG_SAMPLE_COUNT];         // FFT real component
    double vImag[EEG_SAMPLE_COUNT];         // FFT imaginary component
    int sampleIndex = 0;
    
    // Epoch accumulators (average over 30 FFTs)
    float epochDeltaSum, epochThetaSum, epochAlphaSum, epochBetaSum, epochGammaSum;
    int fftCount = 0;
    
    // Time-domain buffer for Hjorth parameters
    float* epochSamples = nullptr;          // All samples in epoch (allocate in PSRAM: 30*256 = 7680 floats)
    int epochSampleCount = 0;
    
    // Display waveform buffer
    float waveformBuf[TFT_SCREEN_WIDTH];    // Circular buffer for display
    int waveformIdx = 0;
    
    BandPowers latestBands;
    float latestFilteredSample = 0.0f;
};
