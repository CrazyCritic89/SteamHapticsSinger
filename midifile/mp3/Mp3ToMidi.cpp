#include "Mp3ToMidi.h"
#include "Mp3Decoder.h"
#include "PitchDetector.h"
#include "MidiExporter.h"

#include <iostream>

bool Mp3ToMidi_Convert(
    const std::string& inputMp3,
    const std::string& outputMid,
    const PitchConfig& config,
    int                bpm)
{
    std::cout << "[Mp3ToMidi] ----------------------------------------\n"
              << "[Mp3ToMidi] Input   : " << inputMp3  << "\n"
              << "[Mp3ToMidi] Output  : " << outputMid << "\n"
              << "[Mp3ToMidi] BPM     : " << bpm       << "\n"
              << "[Mp3ToMidi] ----------------------------------------"
              << std::endl;

    PcmData pcm;
    if (!Mp3_Decode(inputMp3, pcm)) {
        std::cerr << "[Mp3ToMidi] Stage 1 failed: MP3 decoding error." << std::endl;
        return false;
    }

    int sampleRate = pcm.sampleRate;
    std::vector<float> mono = Pcm_StereoToMono(pcm);
    pcm.samples.clear();
    pcm.samples.shrink_to_fit();

    std::vector<NoteEvent> notes = PitchDetect_Run(mono, sampleRate, config);
    mono.clear();
    mono.shrink_to_fit();

    if (notes.empty()) {
        std::cerr << "[Mp3ToMidi] Stage 3: no notes detected.\n"
                  << "            Tips: lower --threshold, widen --minfreq/--maxfreq,\n"
                  << "            or raise --silencedb if the recording is quiet."
                  << std::endl;
        return false;
    }

    if (!MidiExport_Write(notes, outputMid, bpm)) {
        std::cerr << "[Mp3ToMidi] Stage 4 failed: could not write MIDI file." << std::endl;
        return false;
    }

    std::cout << "[Mp3ToMidi] Done - " << notes.size()
              << " notes written to " << outputMid << std::endl;
    return true;
}
