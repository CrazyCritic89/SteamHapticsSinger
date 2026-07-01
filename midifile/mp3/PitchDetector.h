#pragma once

#include <vector>

enum class PitchAlgo {
    YIN      = 0,
    PYIN,
    MPM,
    AUTOCORR,
    AMDF,
    HPS,
    CEPSTRUM,
    FFT_PEAK,
    CQT,
    STFT
};

struct NoteEvent {
    int    midiNote;
    double startSec;
    double endSec;
    int    velocity;
};

struct PitchConfig {
    int       frameSize = 2048;
    int       hopSize   = 512;
    float     threshold = 0.15f;
    float     minFreq   = 50.0f;
    float     maxFreq   = 2000.f;
    float     silenceDb = -40.f;
    PitchAlgo algo      = PitchAlgo::YIN;
};

std::vector<NoteEvent> PitchDetect_Run(
    const std::vector<float>& mono,
    int                       sampleRate,
    const PitchConfig&        config = PitchConfig{});

int   FreqToMidiNote(float freqHz);
float MidiNoteToFreq(int midiNote);
