#include "Amdf.h"

#include <cmath>
#include <vector>

float Amdf_Estimate(const float* frame, int frameSize, int sampleRate,
                    float minFreq, float maxFreq, float threshold)
{
    int minTau = static_cast<int>(static_cast<float>(sampleRate) / maxFreq);
    int maxTau = static_cast<int>(static_cast<float>(sampleRate) / minFreq);

    if (minTau < 1)          minTau = 1;
    if (maxTau >= frameSize) maxTau = frameSize - 1;
    if (maxTau < minTau)     return 0.0f;

    // Average Magnitude Difference Function
    std::vector<float> amdf(static_cast<size_t>(maxTau + 1), 0.0f);
    for (int tau = minTau; tau <= maxTau; ++tau) {
        int    N = frameSize - tau;
        double s = 0.0;
        for (int j = 0; j < N; ++j)
            s += std::abs(static_cast<double>(frame[j]) - frame[j + tau]);
        amdf[tau] = static_cast<float>(s / N);
    }

    // Normalize: compute the max AMDF value in range
    float maxAmdf = 0.0f;
    for (int tau = minTau; tau <= maxTau; ++tau)
        if (amdf[tau] > maxAmdf) maxAmdf = amdf[tau];
    if (maxAmdf < 1e-12f) return 0.0f;

    // Find global minimum
    int   bestTau = minTau;
    float bestVal = amdf[minTau];
    for (int tau = minTau + 1; tau <= maxTau; ++tau) {
        if (amdf[tau] < bestVal) {
            bestVal = amdf[tau];
            bestTau = tau;
        }
    }

    // Accept only if the minimum is clearly below the maximum (voiced condition)
    if (bestVal > maxAmdf * threshold * 5.0f) return 0.0f;

    // Parabolic refinement on AMDF minimum
    double refined = static_cast<double>(bestTau);
    if (bestTau > minTau && bestTau < maxTau) {
        double s0  = amdf[bestTau - 1];
        double s1  = amdf[bestTau];
        double s2  = amdf[bestTau + 1];
        double den = 2.0 * (s0 - 2.0 * s1 + s2);
        if (std::abs(den) > 1e-12)
            refined = bestTau + (s0 - s2) / den;
    }

    if (refined <= 0.0) return 0.0f;
    return static_cast<float>(sampleRate / refined);
}
