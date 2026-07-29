#include "neural_network.h"
#include <math.h>

bool NeuralNetwork::begin(const char* weightsPath) {
    Serial.println("Initializing Neural Network...");
    bool loadedFromSD = false;
    
    if (weightsPath != nullptr) {
        if (spiMutex != nullptr) {
            xSemaphoreTake(spiMutex, portMAX_DELAY);
        }
        
        File file = SD.open(weightsPath, FILE_READ);
        if (file) {
            size_t expectedBytes = NN_TOTAL_PARAMS * sizeof(float);
            size_t bytesRead = file.read((uint8_t*)weights, expectedBytes);
            file.close();
            if (bytesRead == expectedBytes) {
                loadedFromSD = true;
            }
        }
        
        if (spiMutex != nullptr) {
            xSemaphoreGive(spiMutex);
        }
    }
    
    if (loadedFromSD) {
        Serial.printf("Neural Network weights loaded from SD card (%s)\n", weightsPath);
    } else {
        Serial.println("SD weights unavailable — using embedded default weights in Flash");
        memcpy_P(weights, DEFAULT_NN_WEIGHTS, sizeof(DEFAULT_NN_WEIGHTS));
    }
    
    // Set up pointers
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

void NeuralNetwork::relu(float* x, int n) {
    for (int i = 0; i < n; i++) {
        if (x[i] < 0) {
            x[i] = 0;
        }
    }
}

void NeuralNetwork::softmax(float* x, int n) {
    float maxVal = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] > maxVal) {
            maxVal = x[i];
        }
    }
    
    float sumExp = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = exp(x[i] - maxVal);
        sumExp += x[i];
    }
    
    for (int i = 0; i < n; i++) {
        x[i] /= sumExp;
    }
}

void NeuralNetwork::matmul(const float* input, const float* w, const float* b,
                           float* output, int inSize, int outSize) {
    for (int j = 0; j < outSize; j++) {
        float sum = b[j];
        for (int i = 0; i < inSize; i++) {
            // Row-major: weight[output_neuron][input_neuron]
            sum += input[i] * w[j * inSize + i];
        }
        output[j] = sum;
    }
}

SleepStage NeuralNetwork::classify(const float* features) {
    if (!loaded) return STAGE_UNKNOWN;
    
    float out1[NN_HIDDEN1_SIZE];
    float out2[NN_HIDDEN2_SIZE];
    
    // Layer 1
    matmul(features, w1, b1, out1, NN_INPUT_SIZE, NN_HIDDEN1_SIZE);
    relu(out1, NN_HIDDEN1_SIZE);
    
    // Layer 2
    matmul(out1, w2, b2, out2, NN_HIDDEN1_SIZE, NN_HIDDEN2_SIZE);
    relu(out2, NN_HIDDEN2_SIZE);
    
    // Layer 3 (Output)
    matmul(out2, w3, b3, confidences, NN_HIDDEN2_SIZE, NN_OUTPUT_SIZE);
    softmax(confidences, NN_OUTPUT_SIZE);
    
    // Argmax
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

const float* NeuralNetwork::getConfidences() {
    return confidences;
}

bool NeuralNetwork::isLoaded() {
    return loaded;
}
