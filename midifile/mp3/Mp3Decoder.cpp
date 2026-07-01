#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3.h"

#include "Mp3Decoder.h"

#include <fstream>
#include <iostream>
#include <cstdint>

bool Mp3_Decode(const std::string& filePath, PcmData& out)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[Mp3Decoder] Cannot open: " << filePath << std::endl;
        return false;
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        std::cerr << "[Mp3Decoder] Empty or unreadable file: " << filePath << std::endl;
        return false;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> mp3Data(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(mp3Data.data()), fileSize)) {
        std::cerr << "[Mp3Decoder] Read error: " << filePath << std::endl;
        return false;
    }
    file.close();

    mp3dec_t            dec;
    mp3dec_frame_info_t info;
    float               frameBuf[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_init(&dec);

    const uint8_t* ptr       = mp3Data.data();
    int            remaining = static_cast<int>(fileSize);

    out.samples.clear();
    out.sampleRate  = 0;
    out.channels    = 0;
    out.totalFrames = 0;

    while (remaining > 0) {
        int decoded = mp3dec_decode_frame(&dec, ptr, remaining, frameBuf, &info);

        if (info.frame_bytes == 0)
            break;

        ptr       += info.frame_bytes;
        remaining -= info.frame_bytes;

        if (decoded > 0) {
            if (out.sampleRate == 0) {
                out.sampleRate = info.hz;
                out.channels   = info.channels;
            }
            int count = decoded * info.channels;
            out.samples.insert(out.samples.end(), frameBuf, frameBuf + count);
        }
    }

    if (out.channels > 0)
        out.totalFrames = static_cast<int>(out.samples.size()) / out.channels;

    if (out.totalFrames == 0) {
        std::cerr << "[Mp3Decoder] No audio decoded from: " << filePath << std::endl;
        return false;
    }

    std::cout << "[Mp3Decoder] Decoded " << out.totalFrames
              << " frames  |  " << out.sampleRate
              << " Hz  |  " << out.channels << " ch" << std::endl;
    return true;
}

std::vector<float> Pcm_StereoToMono(const PcmData& pcm)
{
    if (pcm.channels <= 1)
        return pcm.samples;

    std::vector<float> mono;
    mono.reserve(static_cast<size_t>(pcm.totalFrames));

    float scale = 1.0f / static_cast<float>(pcm.channels);
    for (int i = 0; i < pcm.totalFrames; ++i) {
        float sum = 0.0f;
        for (int ch = 0; ch < pcm.channels; ++ch)
            sum += pcm.samples[static_cast<size_t>(i * pcm.channels + ch)];
        mono.push_back(sum * scale);
    }
    return mono;
}
