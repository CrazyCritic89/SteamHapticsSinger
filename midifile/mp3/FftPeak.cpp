#include "FftPeak.h"
#include "FftUtil.h"

#include <cmath>
#include <vector>

float FftPeak_Estimate(const float* frame, int frameSize, int sampleRate,
                       float minFreq, float maxFreq, float threshold)
{
    std::vector<float> mag = FftUtil_MagnitudeSpectrum(frame, frameSize);
    int halfSize = static_cast<int>(mag.size());

    int fftSize   = halfSize * 2;
    float freqRes = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    int minBin = static_cast<int>(minFreq / freqRes);
    int maxBin = static_cast<int>(maxFreq / freqRes);
    if (minBin < 1)         minBin = 1;
    if (maxBin >= halfSize) maxBin = halfSize - 1;

    // Find global max magnitude in range for normalisation
    float maxMag = 0.0f;
    for (int k = minBin; k <= maxBin; ++k)
        if (mag[k] > maxMag) maxMag = mag[k];
    if (maxMag < 1e-12f) return 0.0f;

    // Find the lowest-frequency local peak that reaches threshold * maxMag
    int bestBin = -1;
    for (int k = minBin + 1; k < maxBin; ++k) {
        if (mag[k] >= threshold * maxMag &&
            mag[k] >= mag[k - 1] &&
            mag[k] >= mag[k + 1]) {
            bestBin = k;
            break;
        }
    }
    if (bestBin < 0) return 0.0f;

    // Parabolic refinement on magnitude peak
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
