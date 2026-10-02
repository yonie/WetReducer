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
// Prototype parameters: no panel yet, the host's own sliders drive these.
//------------------------------------------------------------------------
enum WetReducerParams : Steinberg::Vst::ParamID
{
    kLowParam      = 0,     // band knobs, 0..1: original -> sine blend -> square
    kLowMidParam   = 1,
    kHighMidParam  = 2,
    kHighParam     = 3,
    kXover1Param   = 4,     // LF<>LMF:  50 /  70 /  90 Hz
    kXover2Param   = 5,     // LMF<>HMF:  1 /   3 /   5 kHz
    kXover3Param   = 6,     // HMF<>HF:   5 /   8 /  10 kHz

    kParamCount    = 7
};

//------------------------------------------------------------------------
} // namespace Yonie
