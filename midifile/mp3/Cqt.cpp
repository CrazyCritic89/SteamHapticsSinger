#include "Cqt.h"

#include <cmath>

float Cqt_Estimate(const float* frame, int frameSize, int sampleRate,
                   float minFreq, float maxFreq, float threshold)
{
    // Constant-Q Transform: evaluate DFT energy at log-spaced (MIDI) frequencies.
    // Each bin aligns exactly with a musical pitch, giving natural resolution.
    const float PI2 = 2.0f * 3.14159265358979323846f;

    // MIDI note range covering [minFreq, maxFreq]
    int minNote = static_cast<int>(69.0f + 12.0f * std::log2f(minFreq / 440.0f));
    int maxNote = static_cast<int>(69.0f + 12.0f * std::log2f(maxFreq / 440.0f)) + 1;
    if (minNote < 0)   minNote = 0;
    if (maxNote > 127) maxNote = 127;

    int   bestNote   = -1;
    float bestEnergy = 0.0f;

    for (int note = minNote; note <= maxNote; ++note) {
        float freq  = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
        float omega = PI2 * freq / static_cast<float>(sampleRate);

        double real = 0.0, imag = 0.0;
        for (int j = 0; j < frameSize; ++j) {
            // Hann-windowed Goertzel DFT at this frequency
            float w   = 0.5f * (1.0f - std::cos(PI2 * j / (frameSize - 1)));
            real += static_cast<double>(frame[j] * w) * std::cos(j * omega);
            imag += static_cast<double>(frame[j] * w) * std::sin(j * omega);
        }

        float energy = static_cast<float>((real * real + imag * imag) / frameSize);
        if (energy > bestEnergy) {
            bestEnergy = energy;
            bestNote   = note;
        }
    }

    if (bestNote < 0 || bestEnergy < 1e-20f) return 0.0f;
    float freq = 440.0f * std::pow(2.0f, (bestNote - 69) / 12.0f);
    if (freq < minFreq || freq > maxFreq) return 0.0f;
    return freq;
}
