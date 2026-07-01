#pragma once

#include "PitchDetector.h"

#include <vector>
#include <string>

bool MidiExport_Write(
    const std::vector<NoteEvent>& events,
    const std::string&            outputPath,
    int                           bpm = 120);
