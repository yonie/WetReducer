//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Yonie {

//------------------------------------------------------------------------
static const Steinberg::FUID kWetReducerProcessorUID (0x47FE5F00, 0x820F48D9, 0xB73918BE, 0xA26B7C13);
static const Steinberg::FUID kWetReducerControllerUID (0x2B17CF3E, 0x1CF94E87, 0x8573649B, 0xAA8876F9);

#define WetReducerVST3Category "Fx|Pitch Shift"

//------------------------------------------------------------------------
// Prototype parameters. Five bands since 2026-10-02 - BASS, BODY, VOICE, PRESENCE, AIR; the
// four-band layout is gone and its sessions load with defaults.
//------------------------------------------------------------------------
enum WetReducerParams : Steinberg::Vst::ParamID
{
    kBandParam     = 0,     // 0..4: band amounts, 0..1 = original -> sine blend -> square
    kXoverParam    = 5,     // 5..8: crossovers, 30 Hz - 16 kHz on a log scale, kept in order
    kSoloParam     = 9,     // 9..13: solo per band; with any on, only the soloed bands are heard
    kResponseParam = 14,    // SLOW / FAST: how quickly the tones follow the music
    kLedParam      = 15,    // 15..19: read-only, each band's lamp, from the processor to the panel
    kLowEdgeParam  = 20,    // where BASS starts, B0..B9; below it the music passes as it is. Off at the bottom
    kHighEdgeParam = 21,    // where AIR ends, B0..B9; above it the music passes as it is. Off at the top
    kVoicesParam   = 22,    // tones per band, 1..8; the VOICES display with its up and down buttons
    kInMeterL      = 23,    // 23..26: read-only, IN L/R and OUT L/R peak, dB-encoded (meterlevel.h)
    kInMeterR      = 24,
    kOutMeterL     = 25,
    kOutMeterR     = 26,

    kParamCount    = 27
};

//------------------------------------------------------------------------
} // namespace Yonie
