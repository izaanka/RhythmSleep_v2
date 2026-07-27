#pragma once

#include "config.h"
#include <SD.h>

class NeuralNetwork {
public:
    bool begin(const char* weightsPath);   // Load weights from SD card binary file
    SleepStage classify(const float* features);  // Run inference on 16 features
    const float* getConfidences();                // Get softmax output [4]
    bool isLoaded();                              // True if weights loaded successfully
    
private:
    void relu(float* x, int n);                   // In-place ReLU activation
    void softmax(float* x, int n);                // In-place softmax
    void matmul(const float* input, const float* weights, const float* bias,
                float* output, int inSize, int outSize);  // Dense layer
    
    // Weights stored contiguously: [L1_W, L1_B, L2_W, L2_B, L3_W, L3_B]
    float weights[NN_TOTAL_PARAMS];
    float confidences[NN_OUTPUT_SIZE] = {0};
    bool loaded = false;
    
    // Pointers into weights array
    float* w1; float* b1;   // Layer 1: 16×32 weights + 32 biases
    float* w2; float* b2;   // Layer 2: 32×16 weights + 16 biases  
    float* w3; float* b3;   // Layer 3: 16×4 weights + 4 biases
};
