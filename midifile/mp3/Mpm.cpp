#include "Mpm.h"

#include <cmath>
#include <vector>

float Mpm_Estimate(const float* frame, int frameSize, int sampleRate,
                   float minFreq, float maxFreq, float threshold)
{
    int minTau = static_cast<int>(static_cast<float>(sampleRate) / maxFreq);
    int maxTau = static_cast<int>(static_cast<float>(sampleRate) / minFreq);

    if (minTau < 1)          minTau = 1;
    if (maxTau >= frameSize) maxTau = frameSize - 1;
    if (maxTau < minTau)     return 0.0f;

    // Normalized Square Difference Function (McLeod)
    std::vector<float> nsdf(static_cast<size_t>(maxTau + 1), 0.0f);
    for (int tau = 0; tau <= maxTau; ++tau) {
        int    N    = frameSize - tau;
        double acf  = 0.0;
        double e0   = 0.0;
        double etau = 0.0;
        for (int j = 0; j < N; ++j) {
            acf  += static_cast<double>(frame[j]) * frame[j + tau];
            e0   += static_cast<double>(frame[j]) * frame[j];
            etau += static_cast<double>(frame[j + tau]) * frame[j + tau];
        }
        double denom = e0 + etau;
        nsdf[tau] = (denom > 1e-12) ? static_cast<float>(2.0 * acf / denom) : 0.0f;
    }

    // Find the first peak in [minTau, maxTau] that surpasses the threshold
    int   bestTau = -1;
    float bestVal = -2.0f;
    bool  inPeak  = false;
    for (int tau = minTau; tau <= maxTau; ++tau) {
        if (!inPeak && nsdf[tau] > threshold) inPeak = true;
        if (inPeak) {
            if (nsdf[tau] > bestVal) {
                bestVal = nsdf[tau];
                bestTau = tau;
            }
            if (tau + 1 <= maxTau && nsdf[tau + 1] < nsdf[tau])
                break;
        }
    }
    if (bestTau < 0) return 0.0f;

    // Parabolic refinement on NSDF maximum
    double refined = static_cast<double>(bestTau);
    if (bestTau > 0 && bestTau < maxTau) {
        double s0  = nsdf[bestTau - 1];
        double s1  = nsdf[bestTau];
        double s2  = nsdf[bestTau + 1];
        double den = 2.0 * (s0 - 2.0 * s1 + s2);
        if (std::abs(den) > 1e-12)
            refined = bestTau + (s0 - s2) / den;
    }

    if (refined <= 0.0) return 0.0f;
    return static_cast<float>(sampleRate / refined);
}
