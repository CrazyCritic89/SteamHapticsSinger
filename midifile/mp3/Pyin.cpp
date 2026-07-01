#include "Pyin.h"

#include <cmath>
#include <vector>

float Pyin_Estimate(const float* frame, int frameSize, int sampleRate,
                    float minFreq, float maxFreq, float threshold)
{
    int minTau = static_cast<int>(static_cast<float>(sampleRate) / maxFreq);
    int maxTau = static_cast<int>(static_cast<float>(sampleRate) / minFreq);

    if (minTau < 2)              minTau = 2;
    if (maxTau >= frameSize / 2) maxTau = frameSize / 2 - 1;
    if (maxTau < minTau)         return 0.0f;

    int W = frameSize - maxTau;
    std::vector<double> d(static_cast<size_t>(maxTau + 1), 0.0);

    for (int tau = 1; tau <= maxTau; ++tau) {
        double s = 0.0;
        for (int j = 0; j < W; ++j) {
            double diff = static_cast<double>(frame[j]) -
                          static_cast<double>(frame[j + tau]);
            s += diff * diff;
        }
        d[tau] = s;
    }

    std::vector<double> cmnd(static_cast<size_t>(maxTau + 1), 1.0);
    double runSum = 0.0;
    for (int tau = 1; tau <= maxTau; ++tau) {
        runSum   += d[tau];
        cmnd[tau] = (runSum > 0.0) ? (d[tau] * tau / runSum) : 1.0;
    }

    // pYIN: collect all local minima below each of N threshold levels,
    // weight each candidate by the Beta(2,18) distribution evaluated at
    // the aperiodicity measure (cmnd value).
    const int N_TH = 100;
    int    bestTau    = -1;
    double bestWeight = 0.0;

    for (int k = 1; k < N_TH; ++k) {
        float th = static_cast<float>(k) / static_cast<float>(N_TH);
        for (int tau = minTau + 1; tau < maxTau; ++tau) {
            if (cmnd[tau] < static_cast<double>(th) &&
                cmnd[tau] <= cmnd[tau - 1] &&
                cmnd[tau] <= cmnd[tau + 1]) {
                // Beta(2,18): weight ∝ a^1 * (1-a)^17
                double a      = std::max(0.0, std::min(1.0, cmnd[tau]));
                double weight = a * std::pow(1.0 - a, 17.0);
                if (weight > bestWeight) {
                    bestWeight = weight;
                    bestTau    = tau;
                }
                break;
            }
        }
    }

    if (bestTau < minTau || bestWeight < 1e-30) return 0.0f;

    double refined = static_cast<double>(bestTau);
    if (bestTau > minTau && bestTau < maxTau) {
        double s0  = cmnd[bestTau - 1];
        double s1  = cmnd[bestTau];
        double s2  = cmnd[bestTau + 1];
        double den = 2.0 * (s0 - 2.0 * s1 + s2);
        if (std::abs(den) > 1e-12)
            refined = bestTau + (s0 - s2) / den;
    }

    if (refined <= 0.0) return 0.0f;
    return static_cast<float>(sampleRate / refined);
}
