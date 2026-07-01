#pragma once

float Autocorr_Estimate(const float* frame, int frameSize, int sampleRate,
                        float minFreq, float maxFreq, float threshold);
