#include "Hps.h"
#include "FftUtil.h"

#include <cmath>
#include <vector>

float Hps_Estimate(const float* frame, int frameSize, int sampleRate,
                   float minFreq, float maxFreq, float threshold)
{
    std::vector<float> mag = FftUtil_MagnitudeSpectrum(frame, frameSize);
    int halfSize = static_cast<int>(mag.size());

    int fftSize   = halfSize * 2;
    float freqRes = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    int minBin = static_cast<int>(minFreq / freqRes);
    int maxBin = static_cast<int>(maxFreq / freqRes);
    if (minBin < 1)           minBin = 1;
    if (maxBin >= halfSize)   maxBin = halfSize - 1;

    // Harmonic Product Spectrum: multiply magnitude at k, 2k, 3k, 4k, 5k
    const int harmonics = 5;
    std::vector<float> hps(halfSize, 0.0f);
    for (int k = minBin; k <= maxBin; ++k) {
        float prod = mag[k];
        for (int h = 2; h <= harmonics; ++h) {
            int idx = k * h;
            if (idx < halfSize)
                prod *= mag[idx];
            else {
                prod = 0.0f;
                break;
            }
        }
        hps[k] = prod;
    }

    int bestBin = minBin;
    for (int k = minBin + 1; k <= maxBin; ++k)
        if (hps[k] > hps[bestBin]) bestBin = k;

    if (hps[bestBin] < 1e-30f) return 0.0f;

    // Parabolic refinement on HPS peak
    double refined = static_cast<double>(bestBin);
    if (bestBin > minBin && bestBin < maxBin) {
        double s0  = hps[bestBin - 1];
        double s1  = hps[bestBin];
        double s2  = hps[bestBin + 1];
        double den = 2.0 * (s0 - 2.0 * s1 + s2);
        if (std::abs(den) > 1e-12)
            refined = bestBin + (s0 - s2) / den;
    }

    float freq = static_cast<float>(refined * freqRes);
    if (freq < minFreq || freq > maxFreq) return 0.0f;
    return freq;
}
