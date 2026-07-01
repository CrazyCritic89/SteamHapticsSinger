#include "PitchDetector.h"
#include "Yin.h"
#include "Pyin.h"
#include "Mpm.h"
#include "Autocorr.h"
#include "Amdf.h"
#include "Hps.h"
#include "Cepstrum.h"
#include "FftPeak.h"
#include "Cqt.h"
#include "Stft.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>

int FreqToMidiNote(float freqHz)
{
    if (freqHz <= 0.0f) return -1;
    float note = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
    int midi = static_cast<int>(std::round(note));
    return (midi >= 0 && midi <= 127) ? midi : -1;
}

float MidiNoteToFreq(int midiNote)
{
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

static float ComputeRms(const float* buf, int n)
{
    if (n <= 0) return 0.0f;
    double acc = 0.0;
    for (int i = 0; i < n; ++i)
        acc += static_cast<double>(buf[i]) * buf[i];
    return static_cast<float>(std::sqrt(acc / n));
}

static float DispatchEstimate(const float* frame, int frameSize,
                               int sampleRate,
                               float minFreq, float maxFreq,
                               float threshold, PitchAlgo algo)
{
    switch (algo) {
        case PitchAlgo::PYIN:     return Pyin_Estimate    (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::MPM:      return Mpm_Estimate     (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::AUTOCORR: return Autocorr_Estimate(frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::AMDF:     return Amdf_Estimate    (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::HPS:      return Hps_Estimate     (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::CEPSTRUM: return Cepstrum_Estimate(frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::FFT_PEAK: return FftPeak_Estimate (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::CQT:      return Cqt_Estimate     (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        case PitchAlgo::STFT:     return Stft_Estimate    (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
        default:                  return Yin_Estimate     (frame, frameSize, sampleRate, minFreq, maxFreq, threshold);
    }
}

std::vector<NoteEvent> PitchDetect_Run(
    const std::vector<float>& mono,
    int                       sampleRate,
    const PitchConfig&        config)
{
    std::vector<NoteEvent> events;

    if (mono.empty() || sampleRate <= 0 ||
        config.frameSize <= 0 || config.hopSize <= 0)
        return events;

    float silenceRms = std::pow(10.0f, config.silenceDb / 20.0f);

    int numSamples = static_cast<int>(mono.size());
    int numFrames  = (numSamples - config.frameSize) / config.hopSize;
    if (numFrames <= 0) return events;

    double secPerHop = static_cast<double>(config.hopSize) / sampleRate;

    int    currentNote = 0;
    double noteStart   = 0.0;
    double velAccum    = 0.0;
    int    velFrames   = 0;

    for (int f = 0; f < numFrames; ++f) {
        double      timeSec = f * secPerHop;
        const float* frame  = mono.data() +
                              static_cast<size_t>(f * config.hopSize);

        float rms          = ComputeRms(frame, config.frameSize);
        int   detectedNote = 0;

        if (rms >= silenceRms) {
            float freq = DispatchEstimate(frame, config.frameSize, sampleRate,
                                          config.minFreq, config.maxFreq,
                                          config.threshold, config.algo);
            if (freq > 0.0f) {
                int midi = FreqToMidiNote(freq);
                if (midi >= 1) detectedNote = midi;
            }
        }

        if (detectedNote == currentNote) {
            velAccum += static_cast<double>(rms);
            ++velFrames;
        } else {
            if (currentNote > 0 && velFrames > 0) {
                float avgRms = static_cast<float>(velAccum / velFrames);
                int vel = static_cast<int>(avgRms * 127.0f * 8.0f);
                vel = std::max(1, std::min(127, vel));
                events.push_back({ currentNote, noteStart, timeSec, vel });
            }
            currentNote = detectedNote;
            noteStart   = timeSec;
            velAccum    = static_cast<double>(rms);
            velFrames   = 1;
        }
    }

    if (currentNote > 0 && velFrames > 0) {
        float  avgRms = static_cast<float>(velAccum / velFrames);
        int    vel    = static_cast<int>(avgRms * 127.0f * 8.0f);
        vel = std::max(1, std::min(127, vel));
        double endSec = static_cast<double>(numFrames) * secPerHop;
        events.push_back({ currentNote, noteStart, endSec, vel });
    }

    std::cout << "[PitchDetector] " << numFrames << " frames analysed  |  "
              << events.size() << " note events detected." << std::endl;
    return events;
}
