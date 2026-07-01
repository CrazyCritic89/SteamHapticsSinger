#pragma once

float FftPeak_Estimate(const float* frame, int frameSize, int sampleRate,
                       float minFreq, float maxFreq, float threshold);
