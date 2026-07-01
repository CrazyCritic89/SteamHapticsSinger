#include "Cepstrum.h"
#include "FftUtil.h"

#include <cmath>
#include <complex>
#include <vector>

float Cepstrum_Estimate(const float* frame, int frameSize, int sampleRate,
                        float minFreq, float maxFreq, float threshold)
{
    int fftSize = FftUtil_NextPow2(frameSize);

    // Step 1: windowed FFT
    std::vector<std::complex<float>> buf(fftSize, {0.0f, 0.0f});
    for (int i = 0; i < frameSize; ++i) {
        float w = 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979f * i / (frameSize - 1)));
        buf[i] = {frame[i] * w, 0.0f};
    }
    FftUtil_Transform(buf);

    // Step 2: replace each bin with its log magnitude (real cepstrum input)
    for (int i = 0; i < fftSize; ++i) {
        float logMag = std::log(std::abs(buf[i]) + 1e-10f);
        buf[i] = {logMag, 0.0f};
    }

    // Step 3: inverse FFT -> power cepstrum
    FftUtil_InverseTransform(buf);

    // Step 4: find peak quefrency in valid pitch period range
    int minQ = static_cast<int>(static_cast<float>(sampleRate) / maxFreq);
    int maxQ = static_cast<int>(static_cast<float>(sampleRate) / minFreq);
    if (minQ < 1)           minQ = 1;
    if (maxQ >= fftSize / 2) maxQ = fftSize / 2 - 1;
    if (maxQ < minQ)        return 0.0f;

    int   bestQ   = minQ;
    float bestVal = buf[minQ].real();
    for (int q = minQ + 1; q <= maxQ; ++q) {
        if (buf[q].real() > bestVal) {
            bestVal = buf[q].real();
            bestQ   = q;
        }
    }

    if (bestQ <= 0) return 0.0f;

    // Parabolic refinement
    double refined = static_cast<double>(bestQ);
    if (bestQ > minQ && bestQ < maxQ) {
        double s0  = buf[bestQ - 1].real();
        double s1  = buf[bestQ].real();
        double s2  = buf[bestQ + 1].real();
        double den = 2.0 * (s0 - 2.0 * s1 + s2);
        if (std::abs(den) > 1e-12)
            refined = bestQ + (s0 - s2) / den;
    }

    if (refined <= 0.0) return 0.0f;
    float freq = static_cast<float>(sampleRate) / static_cast<float>(refined);
    if (freq < minFreq || freq > maxFreq) return 0.0f;
    return freq;
}
