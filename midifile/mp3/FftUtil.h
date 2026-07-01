#pragma once

#include <cmath>
#include <complex>
#include <vector>

static inline int FftUtil_NextPow2(int n)
{
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

static inline void FftUtil_Transform(std::vector<std::complex<float>>& buf)
{
    int N = static_cast<int>(buf.size());
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(buf[i], buf[j]);
    }
    for (int len = 2; len <= N; len <<= 1) {
        float ang = -2.0f * 3.14159265358979323846f / static_cast<float>(len);
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<float> u = buf[i + j];
                std::complex<float> v = buf[i + j + len / 2] * w;
                buf[i + j]           = u + v;
                buf[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

static inline void FftUtil_InverseTransform(std::vector<std::complex<float>>& buf)
{
    for (auto& c : buf) c = std::conj(c);
    FftUtil_Transform(buf);
    float n = static_cast<float>(buf.size());
    for (auto& c : buf) c = std::conj(c) / n;
}

static inline std::vector<float> FftUtil_MagnitudeSpectrum(
    const float* frame, int N, bool applyHann = true)
{
    int fftSize = FftUtil_NextPow2(N);
    std::vector<std::complex<float>> buf(fftSize, {0.0f, 0.0f});
    for (int i = 0; i < N; ++i) {
        float w = applyHann
            ? 0.5f * (1.0f - std::cos(2.0f * 3.14159265358979f * i / (N - 1)))
            : 1.0f;
        buf[i] = {frame[i] * w, 0.0f};
    }
    FftUtil_Transform(buf);
    int half = fftSize / 2;
    std::vector<float> mag(half);
    for (int i = 0; i < half; ++i)
        mag[i] = std::abs(buf[i]);
    return mag;
}
