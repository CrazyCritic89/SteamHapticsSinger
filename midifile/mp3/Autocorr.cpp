#include "Autocorr.h"

#include <cmath>
#include <vector>

float Autocorr_Estimate(const float* frame, int frameSize, int sampleRate,
                        float minFreq, float maxFreq, float threshold)
{
    int minTau = static_cast<int>(static_cast<float>(sampleRate) / maxFreq);
    int maxTau = static_cast<int>(static_cast<float>(sampleRate) / minFreq);

    if (minTau < 1)          minTau = 1;
    if (maxTau >= frameSize) maxTau = frameSize - 1;
    if (maxTau < minTau)     return 0.0f;

    // Normalized autocorrelation
    double acf0 = 0.0;
    for (int j = 0; j < frameSize; ++j)
        acf0 += static_cast<double>(frame[j]) * frame[j];
    if (acf0 < 1e-12) return 0.0f;

    std::vector<float> acf(static_cast<size_t>(maxTau + 1), 0.0f);
    for (int tau = 0; tau <= maxTau; ++tau) {
        double s = 0.0;
        for (int j = 0; j < frameSize - tau; ++j)
            s += static_cast<double>(frame[j]) * frame[j + tau];
        acf[tau] = static_cast<float>(s / acf0);
    }

    // Skip to first local minimum after tau=0
    int startTau = minTau;
    for (int tau = 1; tau < minTau; ++tau) {
        if (acf[tau] <= acf[tau + 1]) { startTau = tau; break; }
    }

    // Find the highest peak above threshold in the valid range
    int   bestTau = -1;
    float bestVal = -1.0f;
    for (int tau = startTau; tau <= maxTau; ++tau) {
        if (acf[tau] > threshold && acf[tau] > bestVal) {
            bestVal = acf[tau];
            bestTau = tau;
            if (tau + 1 <= maxTau && acf[tau + 1] < acf[tau]) break;
        }
    }
    if (bestTau < 0) return 0.0f;

    // Parabolic refinement on ACF maximum
    double refined = static_cast<double>(bestTau);
    if (bestTau > 1 && bestTau < maxTau) {
        double s0  = acf[bestTau - 1];
        double s1  = acf[bestTau];
        double s2  = acf[bestTau + 1];
        double den = 2.0 * (s0 - 2.0 * s1 + s2);
        if (std::abs(den) > 1e-12)
            refined = bestTau + (s0 - s2) / den;
    }

    if (refined <= 0.0) return 0.0f;
    return static_cast<float>(sampleRate / refined);
}
