#!/usr/bin/env python3
"""
RhythmSleep - Default Neural Network Weight Generator (NumPy version)

Generates a heuristic-based weights.bin for the sleep classification MLP.
Uses NumPy for matrix calculations and binary serialization.

Architecture: Input(16) -> Dense(32, ReLU) -> Dense(16, ReLU) -> Dense(4, Softmax)
Output classes: [WAKE=0, LIGHT=1, DEEP=2, REM=3]

Feature vector (16 elements):
  0: relDelta         5: ratioDelTh       10: spectralEntropy    
  1: relTheta         6: ratioThAl        11: hjorthActivity     
  2: relAlpha         7: ratioThBe        12: hjorthMobility     
  3: relBeta          8: ratioSlowFast    13: hjorthComplexity   
  4: relGamma         9: spectralEdge95   14: rmsVoltage         
                                          15: zeroCrossingRate   

Weight file layout (1140 float32 values):
  L1 weights (512) -> L1 biases (32) -> L2 weights (512) -> L2 biases (16)
  -> L3 weights (64) -> L3 biases (4)

Weights are row-major: weight[output_neuron * input_size + input_neuron]

Usage: python3 generate_default_weights.py [output_path]
"""

import sys
import os
import numpy as np

np.random.seed(42)

# Architecture constants
IN_SIZE = 16
H1_SIZE = 32
H2_SIZE = 16
OUT_SIZE = 4


def build_weights():
    """
    Build heuristic weight matrices using NumPy arrays to encode EEG-sleep correlations.
    
    Weights are row-major matrices: shape (out_features, in_features)
    Biases are 1D arrays: shape (out_features,)
    """
    # Layer 1: 16 -> 32
    w1 = np.random.normal(0.0, 0.05, size=(H1_SIZE, IN_SIZE)).astype(np.float32)
    b1 = np.zeros(H1_SIZE, dtype=np.float32)

    # Neurons 0-7: WAKE detectors
    for i in range(8):
        w1[i, :] = 0.0
        w1[i, 2] = 1.8 + 0.15 * i   # relAlpha
        w1[i, 3] = 1.4 + 0.10 * i   # relBeta
        w1[i, 0] = -1.5              # relDelta
        w1[i, 8] = -1.2              # ratioSlowFast
        w1[i, 15] = 0.6              # zeroCrossingRate
        w1[i, 9] = 0.8               # spectralEdge95
        w1[i, 4] = 0.4               # relGamma
        b1[i] = -0.5

    # Neurons 8-15: LIGHT sleep detectors
    for i in range(8, 16):
        w1[i, :] = 0.0
        w1[i, 1] = 1.6 + 0.15 * (i - 8)  # relTheta
        w1[i, 6] = 1.2                    # ratioThAl
        w1[i, 2] = -0.8                   # relAlpha
        w1[i, 0] = -0.3                   # relDelta
        w1[i, 7] = 0.6                    # ratioThBe
        w1[i, 10] = 0.4                   # spectralEntropy
        w1[i, 9] = 0.3                    # spectralEdge95
        b1[i] = -0.4

    # Neurons 16-23: DEEP sleep detectors
    for i in range(16, 24):
        w1[i, :] = 0.0
        w1[i, 0] = 1.8 + 0.15 * (i - 16)  # relDelta
        w1[i, 5] = 1.0                     # ratioDelTh
        w1[i, 8] = 1.4                     # ratioSlowFast
        w1[i, 2] = -1.5                    # relAlpha
        w1[i, 3] = -1.2                    # relBeta
        w1[i, 9] = -1.0                    # spectralEdge95
        w1[i, 11] = 0.8                    # hjorthActivity
        w1[i, 10] = -0.5                   # spectralEntropy
        b1[i] = -0.6

    # Neurons 24-31: REM detectors
    for i in range(24, 32):
        w1[i, :] = 0.0
        w1[i, 1] = 0.4     # relTheta
        w1[i, 3] = 1.6     # relBeta
        w1[i, 0] = -1.4    # relDelta
        w1[i, 2] = -0.8    # relAlpha
        w1[i, 10] = 1.5    # spectralEntropy
        w1[i, 12] = 0.8    # hjorthMobility
        w1[i, 11] = -0.8   # hjorthActivity
        w1[i, 14] = -0.7   # rmsVoltage
        w1[i, 4] = 0.8     # relGamma
        w1[i, 13] = 0.5    # hjorthComplexity
        w1[i, 6] = -0.3    # ratioThAl
        b1[i] = -0.4

    # Add small Gaussian noise to break symmetry
    w1 += np.random.normal(0.0, 0.03, size=w1.shape).astype(np.float32)

    # Layer 2: 32 -> 16
    w2 = np.random.normal(0.0, 0.1, size=(H2_SIZE, H1_SIZE)).astype(np.float32)
    b2 = np.zeros(H2_SIZE, dtype=np.float32)

    for cls in range(4):
        h1_start = cls * 8
        for j in range(4):
            n = cls * 4 + j
            w2[n, :] = 0.0
            for k in range(8):
                w2[n, h1_start + k] = 0.4 + 0.05 * k
            for other in range(4):
                if other != cls:
                    for k in range(8):
                        w2[n, other * 8 + k] = -0.1
            b2[n] = -0.2

    w2 += np.random.normal(0.0, 0.03, size=w2.shape).astype(np.float32)

    # Layer 3: 16 -> 4
    w3 = np.random.normal(0.0, 0.1, size=(OUT_SIZE, H2_SIZE)).astype(np.float32)
    b3 = np.zeros(OUT_SIZE, dtype=np.float32)

    for cls in range(4):
        w3[cls, :] = -0.2
        for j in range(4):
            w3[cls, cls * 4 + j] = 0.8 + 0.1 * j
        b3[cls] = -0.1

    w3 += np.random.normal(0.0, 0.02, size=w3.shape).astype(np.float32)

    return w1, b1, w2, b2, w3, b3


def forward(x, w1, b1, w2, b2, w3, b3):
    """Run vectorised forward pass through the MLP using NumPy."""
    x = np.asarray(x, dtype=np.float32)
    
    # Layer 1: Dense + ReLU
    h1 = np.maximum(0.0, np.dot(w1, x) + b1)
    
    # Layer 2: Dense + ReLU
    h2 = np.maximum(0.0, np.dot(w2, h1) + b2)
    
    # Layer 3: Dense + Softmax
    logits = np.dot(w3, h2) + b3
    exp_logits = np.exp(logits - np.max(logits))
    probs = exp_logits / np.sum(exp_logits)
    
    return probs


def verify(w1, b1, w2, b2, w3, b3):
    """Test with synthetic feature vectors."""
    tests = {
        "WAKE":  [0.05, 0.10, 0.45, 0.30, 0.10, 0.10, 0.08, 0.11, 0.06, 0.85, 0.70, 0.15, 0.50, 0.45, 0.55, 0.80],
        "LIGHT": [0.15, 0.40, 0.15, 0.18, 0.12, 0.08, 0.75, 0.60, 0.40, 0.50, 0.55, 0.30, 0.35, 0.55, 0.35, 0.50],
        "DEEP":  [0.65, 0.12, 0.04, 0.10, 0.09, 0.85, 0.90, 0.40, 0.90, 0.15, 0.30, 0.85, 0.15, 0.30, 0.80, 0.20],
        "REM":   [0.08, 0.28, 0.08, 0.35, 0.21, 0.06, 0.75, 0.25, 0.12, 0.65, 0.85, 0.10, 0.65, 0.70, 0.25, 0.60],
    }
    labels = ["WAKE", "LIGHT", "DEEP", "REM"]

    print("\n--- Forward Pass Verification ---")
    for name, feat in tests.items():
        probs = forward(feat, w1, b1, w2, b2, w3, b3)
        pred_idx = np.argmax(probs)
        pred = labels[pred_idx]
        ok = "[OK]" if pred == name else "[FAIL]"
        ps = " ".join(f"{labels[i][0]}:{probs[i]:.2f}" for i in range(4))
        print(f"  {ok} Input={name:5s} -> Predicted={pred:5s}  [{ps}]")


def save(path, w1, b1, w2, b2, w3, b3):
    """Write flat binary float32 file using NumPy tofile."""
    all_params = np.concatenate([
        w1.flatten(), b1.flatten(),
        w2.flatten(), b2.flatten(),
        w3.flatten(), b3.flatten()
    ]).astype(np.float32)

    expected = (IN_SIZE * H1_SIZE + H1_SIZE) + (H1_SIZE * H2_SIZE + H2_SIZE) + (H2_SIZE * OUT_SIZE + OUT_SIZE)
    assert len(all_params) == expected, f"{len(all_params)} != {expected}"

    out_dir = os.path.dirname(path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    all_params.tofile(path)
    print(f"\n--- Saved {len(all_params)} params ({len(all_params)*4} bytes) -> {path}")


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "sd_card_contents/model/weights.bin"

    print("==========================================================")
    print("   RhythmSleep - Default Neural Network Weight Generator  ")
    print("==========================================================")
    print(f"\nArchitecture: {IN_SIZE} -> {H1_SIZE} -> {H2_SIZE} -> {OUT_SIZE}")
    total = (IN_SIZE * H1_SIZE + H1_SIZE) + (H1_SIZE * H2_SIZE + H2_SIZE) + (H2_SIZE * OUT_SIZE + OUT_SIZE)
    print(f"Total parameters: {total}")

    w1, b1, w2, b2, w3, b3 = build_weights()

    print("\nWeight counts:")
    print(f"  L1: W={w1.shape}={w1.size}, b={b1.size}")
    print(f"  L2: W={w2.shape}={w2.size}, b={b2.size}")
    print(f"  L3: W={w3.shape}={w3.size}, b={b3.size}")

    verify(w1, b1, w2, b2, w3, b3)
    save(out, w1, b1, w2, b2, w3, b3)

    print(f"\nCopy '{out}' to your SD card at /model/weights.bin")


if __name__ == "__main__":
    main()
