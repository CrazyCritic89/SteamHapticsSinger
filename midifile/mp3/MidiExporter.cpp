#include "MidiExporter.h"
#include "../midifile.h"

#include <algorithm>
#include <iostream>

bool MidiExport_Write(
    const std::vector<NoteEvent>& events,
    const std::string&            outputPath,
    int                           bpm)
{
    if (events.empty()) {
        std::cerr << "[MidiExporter] No events to write." << std::endl;
        return false;
    }

    const int PPQ = 480;
    MidiFile_t midi = MidiFile_new(0, MIDI_FILE_DIVISION_TYPE_PPQ, PPQ);
    if (!midi) {
        std::cerr << "[MidiExporter] Failed to allocate MIDI object." << std::endl;
        return false;
    }

    MidiFileTrack_t track = MidiFile_createTrack(midi);

    MidiFileTrack_createTempoEvent(track, 0L, static_cast<float>(bpm));

    double ticksPerSec = static_cast<double>(PPQ * bpm) / 60.0;

    long minDuration = PPQ / 4;

    for (const auto& ev : events) {
        if (ev.midiNote < 1 || ev.midiNote > 127) continue;

        long startTick = static_cast<long>(ev.startSec * ticksPerSec);
        long endTick   = static_cast<long>(ev.endSec   * ticksPerSec);

        if (endTick - startTick < minDuration)
            endTick = startTick + minDuration;

        int vel = std::max(1, std::min(127, ev.velocity));

        MidiFileTrack_createNoteStartAndEndEvents(
            track,
            startTick, endTick,
            0,
            ev.midiNote,
            vel,
            0
        );
    }

    int result = MidiFile_save(midi, outputPath.c_str());
    MidiFile_free(midi);

    if (result < 0) {
        std::cerr << "[MidiExporter] Failed to save: " << outputPath << std::endl;
        return false;
    }

    std::cout << "[MidiExporter] " << events.size()
              << " notes -> " << outputPath << std::endl;
    return true;
}
