#include "Stft.h"
#include "FftUtil.h"

#include <cmath>
#include <complex>
#include <vector>

float Stft_Estimate(const float* frame, int frameSize, int sampleRate,
                    float minFreq, float maxFreq, float threshold)
{
    // STFT with Blackman window + harmonic scoring.
    // Scores each candidate fundamental by summing its harmonic energies.
    const float PI  = 3.14159265358979323846f;
    int fftSize     = FftUtil_NextPow2(frameSize);

    std::vector<std::complex<float>> buf(fftSize, {0.0f, 0.0f});
    for (int i = 0; i < frameSize; ++i) {
        float w = 0.42f
                  - 0.50f * std::cos(2.0f * PI * i / (frameSize - 1))
                  + 0.08f * std::cos(4.0f * PI * i / (frameSize - 1));
        buf[i] = {frame[i] * w, 0.0f};
    }
    FftUtil_Transform(buf);

    int half      = fftSize / 2;
    float freqRes = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    std::vector<float> mag(half);
    for (int k = 0; k < half; ++k) mag[k] = std::abs(buf[k]);

    int minBin = static_cast<int>(minFreq / freqRes);
    int maxBin = static_cast<int>(maxFreq / freqRes);
    if (minBin < 1)       minBin = 1;
    if (maxBin >= half)   maxBin = half - 1;

    // Score each local peak in range by summing the first 4 harmonic magnitudes
    int   bestBin   = -1;
    float bestScore = 0.0f;
    for (int k = minBin + 1; k < maxBin; ++k) {
        if (mag[k] <= mag[k - 1] || mag[k] <= mag[k + 1]) continue;
        float score = 0.0f;
        for (int h = 1; h <= 4; ++h) {
            int hk = k * h;
            if (hk < half) score += mag[hk];
        }
        if (score > bestScore) {
            bestScore = score;
            bestBin   = k;
        }
    }
    if (bestBin < 0) return 0.0f;

    // Parabolic refinement
    double refined = static_cast<double>(bestBin);
    if (bestBin > minBin && bestBin < maxBin) {
        double s0  = mag[bestBin - 1];
        double s1  = mag[bestBin];
        double s2  = mag[bestBin + 1];
        double den = 2.0 * (s0 - 2.0 * s1 + s2);
        if (std::abs(den) > 1e-12)
            refined = bestBin + (s0 - s2) / den;
    }

    float freq = static_cast<float>(refined * freqRes);
    if (freq < minFreq || freq > maxFreq) return 0.0f;
    return freq;
}
