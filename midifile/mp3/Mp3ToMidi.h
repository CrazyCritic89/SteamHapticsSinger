#pragma once

#include "PitchDetector.h"

#include <string>

bool Mp3ToMidi_Convert(
    const std::string& inputMp3,
    const std::string& outputMid,
    const PitchConfig& config = PitchConfig{},
    int                bpm    = 120);
