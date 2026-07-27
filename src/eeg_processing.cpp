#include "eeg_processing.h"
#include <arduinoFFT.h>

// Create an instance of ArduinoFFT
ArduinoFFT<double> FFT = ArduinoFFT<double>();

void EEGProcessor::begin() {
    Serial.println("Initializing EEGProcessor...");
    
    // Allocate epoch buffer in PSRAM
    // epochSamples needs to hold 30 * 256 = 7680 floats
    size_t bufferSize = EEG_FFTS_PER_EPOCH * EEG_SAMPLE_COUNT * sizeof(float);
    epochSamples = (float*)ps_malloc(bufferSize);
    
    if (epochSamples == nullptr) {
        Serial.println("ERROR: Failed to allocate epochSamples in PSRAM!");
    } else {
        Serial.printf("Successfully allocated %u bytes in PSRAM for epochSamples\n", bufferSize);
    }
    
    // Initialize variables
    epochSampleCount = 0;
    resetEpoch();
    resetFFTBuffer();
    
    for (int i = 0; i < TFT_SCREEN_WIDTH; i++) {
        waveformBuf[i] = 0;
    }
}

float EEGProcessor::bandpassFilter(float input) {
    // 4th-order Butterworth 0.5-45 Hz at Fs=256 Hz
    // Cascaded biquad (SOS) sections
    float output;
    
    // Section 1: High-pass component
    static float z1_1 = 0, z2_1 = 0;
    float x1 = input - (-1.97278f * z1_1) - (0.97298f * z2_1);
    output = 0.98639f * x1 + (-1.97278f * z1_1) + 0.98639f * z2_1;
    z2_1 = z1_1; z1_1 = x1;

    // Section 2: Low-pass component
    static float z1_2 = 0, z2_2 = 0;
    float x2 = output - (-1.47549f * z1_2) - (0.58691f * z2_2);
    output = 1.0f * x2 + 2.0f * z1_2 + 1.0f * z2_2;
    z2_2 = z1_2; z1_2 = x2;

    // Section 3
    static float z1_3 = 0, z2_3 = 0;
    float x3 = output - (-1.70096f * z1_3) - (0.78170f * z2_3);
    output = 1.0f * x3 + (-2.0f * z1_3) + 1.0f * z2_3;
    z2_3 = z1_3; z1_3 = x3;

    // Section 4
    static float z1_4 = 0, z2_4 = 0;
    float x4 = output - (-1.88437f * z1_4) - (0.89257f * z2_4);
    output = 0.02786f * x4 + 0.05573f * z1_4 + 0.02786f * z2_4;
    z2_4 = z1_4; z1_4 = x4;
    
    return output;
}

void EEGProcessor::collectSample() {
    // Read ADC, raw value 12-bit (0-4095)
    int rawValue = analogRead(PIN_EEG_ADC);
    
    // Center it
    float centered = (float)rawValue - EEG_ADC_MIDPOINT;
    
    // Apply filter
    float filtered = bandpassFilter(centered);
    latestFilteredSample = filtered;
    
    // Store in FFT buffer
    if (sampleIndex < EEG_SAMPLE_COUNT) {
        vReal[sampleIndex] = filtered;
        vImag[sampleIndex] = 0.0;
        sampleIndex++;
    }
    
    // Store in epoch buffer
    if (epochSamples != nullptr && epochSampleCount < (EEG_FFTS_PER_EPOCH * EEG_SAMPLE_COUNT)) {
        epochSamples[epochSampleCount++] = filtered;
    }
    
    // Store in waveform buffer for display
    waveformBuf[waveformIdx] = filtered;
    waveformIdx = (waveformIdx + 1) % TFT_SCREEN_WIDTH;
}

bool EEGProcessor::isFFTBufferFull() {
    return sampleIndex >= EEG_SAMPLE_COUNT;
}

void EEGProcessor::computeFFT() {
    if (sampleIndex < EEG_SAMPLE_COUNT) return;
    
    // Compute FFT using arduinoFFT
    FFT.windowing(vReal, EEG_SAMPLE_COUNT, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, EEG_SAMPLE_COUNT, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, EEG_SAMPLE_COUNT);
    
    // Calculate band powers
    // Resolution = Fs / N = 256 / 256 = 1 Hz per bin
    float delta = 0, theta = 0, alpha = 0, beta = 0, gamma = 0;
    
    for (int i = 1; i <= 45; i++) {
        float power = vReal[i] * vReal[i]; // sum of squared magnitudes
        if (i >= 1 && i < 4) delta += power; // 0.5-4 Hz (using bins 1-4 for simplicity approx)
        else if (i >= 4 && i < 8) theta += power; // 4-8 Hz
        else if (i >= 8 && i < 13) alpha += power; // 8-13 Hz
        else if (i >= 13 && i < 30) beta += power; // 13-30 Hz
        else if (i >= 30 && i <= 45) gamma += power; // 30-45 Hz
    }
    
    float total = delta + theta + alpha + beta + gamma;
    
    // Update latest bands
    latestBands.delta = delta;
    latestBands.theta = theta;
    latestBands.alpha = alpha;
    latestBands.beta = beta;
    latestBands.gamma = gamma;
    latestBands.total = total;
    
    if (total > 0) {
        latestBands.relDelta = delta / total;
        latestBands.relTheta = theta / total;
        latestBands.relAlpha = alpha / total;
        latestBands.relBeta = beta / total;
        latestBands.relGamma = gamma / total;
    } else {
        latestBands.relDelta = latestBands.relTheta = latestBands.relAlpha = latestBands.relBeta = latestBands.relGamma = 0;
    }
    
    // Accumulate for epoch
    epochDeltaSum += delta;
    epochThetaSum += theta;
    epochAlphaSum += alpha;
    epochBetaSum += beta;
    epochGammaSum += gamma;
    
    fftCount++;
    
    resetFFTBuffer();
}

void EEGProcessor::resetFFTBuffer() {
    sampleIndex = 0;
}

void EEGProcessor::resetEpoch() {
    epochDeltaSum = 0;
    epochThetaSum = 0;
    epochAlphaSum = 0;
    epochBetaSum = 0;
    epochGammaSum = 0;
    fftCount = 0;
    epochSampleCount = 0;
}

void EEGProcessor::computeEpochFeatures(EEGFeatures& features) {
    if (fftCount == 0) return;
    
    // Average band powers
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
    
    // Band ratios
    features.ratioDelTh = (features.bands.theta > 0) ? (features.bands.delta / features.bands.theta) : 0;
    features.ratioThAl = (features.bands.alpha > 0) ? (features.bands.theta / features.bands.alpha) : 0;
    features.ratioThBe = (features.bands.beta > 0) ? (features.bands.theta / features.bands.beta) : 0;
    
    float denom = features.bands.alpha + features.bands.beta;
    features.ratioSlowFast = (denom > 0) ? ((features.bands.delta + features.bands.theta) / denom) : 0;
    
    // Time-domain features (Hjorth, RMS, ZCR)
    if (epochSamples != nullptr && epochSampleCount > 0) {
        float sum = 0, sumSq = 0;
        int zeroCrossings = 0;
        float prevSample = epochSamples[0];
        
        float dSum = 0, dSumSq = 0;
        float ddSumSq = 0;
        float prevD = 0;
        
        for (int i = 0; i < epochSampleCount; i++) {
            float s = epochSamples[i];
            sum += s;
            sumSq += s * s;
            
            if (i > 0) {
                if ((s > 0 && prevSample < 0) || (s < 0 && prevSample > 0)) {
                    zeroCrossings++;
                }
                float d = s - prevSample;
                dSum += d;
                dSumSq += d * d;
                
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
        
        float ddVariance = ddSumSq / (epochSampleCount - 2); // approx
        if (ddVariance < 0.0f) ddVariance = 0.0f;

        // Hjorth Parameters
        features.hjorthActivity = variance;
        if (variance > 0) {
            features.hjorthMobility = sqrt(dVariance / variance);
        } else {
            features.hjorthMobility = 0;
        }
        
        if (dVariance > 0 && features.hjorthMobility > 0) {
            float mobilityD = sqrt(ddVariance / dVariance);
            features.hjorthComplexity = mobilityD / features.hjorthMobility;
        } else {
            features.hjorthComplexity = 0;
        }
        
        features.rmsVoltage = sqrt(sumSq / epochSampleCount);
        features.zeroCrossingRate = (float)zeroCrossings / epochSampleCount;
    }
    
    // Spectral features: Approx since we don't save all FFTs
    // Simple placeholder for Spectral Edge 95% and Entropy (would need proper full spectrum averaging)
    // We approximate using the averaged bands
    float p[5] = {features.bands.relDelta, features.bands.relTheta, features.bands.relAlpha, features.bands.relBeta, features.bands.relGamma};
    float entropy = 0;
    float cumulative = 0;
    float edge95 = 0;
    bool edgeFound = false;
    
    float freqs[5] = {4.0f, 8.0f, 13.0f, 30.0f, 45.0f};
    
    for (int i = 0; i < 5; i++) {
        if (p[i] > 0) {
            entropy -= p[i] * log(p[i]);
        }
        cumulative += p[i];
        if (!edgeFound && cumulative >= 0.95f) {
            edge95 = freqs[i];
            edgeFound = true;
        }
    }
    features.spectralEntropy = entropy;
    features.spectralEdge95 = edgeFound ? edge95 : 45.0f;
    
    features.packForNN();
}

float EEGProcessor::getFilteredSample() {
    return latestFilteredSample;
}

const BandPowers& EEGProcessor::getLatestBandPowers() {
    return latestBands;
}

int EEGProcessor::getFFTCount() {
    return fftCount;
}

float* EEGProcessor::getWaveformBuffer() {
    return waveformBuf;
}

int EEGProcessor::getWaveformLength() {
    return TFT_SCREEN_WIDTH;
}

const float* EEGProcessor::getEpochSamples() const {
    return epochSamples;
}

int EEGProcessor::getEpochSampleCount() const {
    return epochSampleCount;
}
