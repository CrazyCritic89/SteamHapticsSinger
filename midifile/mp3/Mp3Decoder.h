#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct PcmData {
    std::vector<float> samples;
    int sampleRate  = 0;
    int channels    = 0;
    int totalFrames = 0;
};

bool Mp3_Decode(const std::string& filePath, PcmData& out);
std::vector<float> Pcm_StereoToMono(const PcmData& pcm);
