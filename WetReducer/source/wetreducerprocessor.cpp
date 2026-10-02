//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetreducerprocessor.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;

namespace Yonie {

WetReducerProcessor::WetReducerProcessor()
{
    setControllerClass(kWetReducerControllerUID);
}

tresult PLUGIN_API WetReducerProcessor::initialize(FUnknown* context)
{
    tresult result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;
    addAudioInput(STR16("Stereo In"), Vst::SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), Vst::SpeakerArr::kStereo);
    return kResultOk;
}

// Prototype: stereo only.
tresult PLUGIN_API WetReducerProcessor::setBusArrangements(Vst::SpeakerArrangement* inputs, int32 numIns,
                                                          Vst::SpeakerArrangement* outputs, int32 numOuts)
{
    if (numIns == 1 && numOuts == 1 && inputs[0] == Vst::SpeakerArr::kStereo && outputs[0] == Vst::SpeakerArr::kStereo)
        return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
    return kResultFalse;
}

tresult PLUGIN_API WetReducerProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
{
    engine.prepare(newSetup.sampleRate);
    engine.setSettings(pending);
    return AudioEffect::setupProcessing(newSetup);
}

tresult PLUGIN_API WetReducerProcessor::setActive(TBool state)
{
    if (state)
        engine.reset();
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API WetReducerProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
    return symbolicSampleSize == Vst::kSample32 ? kResultTrue : kResultFalse;
}

void WetReducerProcessor::apply(Vst::ParamID id, double value)
{
    if (id < kXoverParam)
        pending.amount[id - kBandParam] = std::clamp(value, 0.0, 1.0);
    else if (id < kSoloParam)
        pending.xover[id - kXoverParam] = Reducer::xoverHz(static_cast<int>(id - kXoverParam), value);
    else if (id < kResponseParam)
        pending.solo[id - kSoloParam] = value > 0.5;
    else if (id == kResponseParam)
        pending.response = Reducer::responseIndex(value);
}

// The lamps flash each time a band's tone moves to a new note, brighter the further its knob is
// turned, and fade in about a tenth of a second. While the band has music but holds its note,
// the lamp keeps a faint glow. A band whose knob is at 0, or that another band's solo
// silences, stays dark.
void WetReducerProcessor::sendLeds(Vst::ProcessData& data)
{
    const auto& st = engine.settings();
    bool anySolo = false;
    for (bool on : st.solo)
        anySolo = anySolo || on;
    const double fall = std::exp(-data.numSamples / (0.1 * std::max(1.0, processSetup.sampleRate)));
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        const double db = 20.0 * std::log10(engine.bandLevel(b) + 1e-9);
        const double caught = std::clamp((db + 60.0) / 30.0, 0.0, 1.0);
        // dark while the band's knob is at 0 or another band's solo silences it
        const double weight = (anySolo && !st.solo[b]) ? 0.0 : st.amount[b];
        const int changes = engine.bandNoteChanges(b);
        const bool newNote = changes != seenChanges[b];
        seenChanges[b] = changes;
        double lit = led[b] * fall;
        if (newNote && caught > 0.0)
            lit = weight;
        lit = std::max(lit, 0.12 * caught * weight);
        led[b] = lit;
        if (data.outputParameterChanges && std::abs(led[b] - sentLed[b]) > 0.004)
        {
            int32 index = 0;
            if (auto* q = data.outputParameterChanges->addParameterData(kLedParam + b, index))
            {
                int32 point = 0;
                q->addPoint(0, led[b], point);
                sentLed[b] = led[b];
            }
        }
    }
}

tresult PLUGIN_API WetReducerProcessor::process(Vst::ProcessData& data)
{
    if (data.inputParameterChanges)
    {
        const int32 count = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < count; ++i)
        {
            Vst::IParamValueQueue* q = data.inputParameterChanges->getParameterData(i);
            Vst::ParamValue v;
            int32 offset;
            if (q && q->getPoint(q->getPointCount() - 1, offset, v) == kResultTrue)
                apply(q->getParameterId(), v);
        }
        engine.setSettings(pending);
    }
    if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0)
        return kResultOk;

    float** in = data.inputs[0].channelBuffers32;
    float** out = data.outputs[0].channelBuffers32;
    engine.process(in[0], in[1], out[0], out[1], data.numSamples);
    data.outputs[0].silenceFlags = 0;
    sendLeds(data);
    return kResultOk;
}

// Prototype state. Version 4 is the five-band layout, version 5 adds VERY SLOW as response 0; anything older was a four-band prototype
// and loads with the defaults.
tresult PLUGIN_API WetReducerProcessor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0;
    if (!s.readInt32(version))
        return kResultFalse;
    ReducerEngine::Settings st;
    if (version >= 4)
    {
        for (auto& a : st.amount)
        {
            double v = 0.0;
            if (s.readDouble(v))
                a = std::clamp(v, 0.0, 1.0);
        }
        for (int i = 0; i < Reducer::kXovers; ++i)
        {
            double hz = Reducer::kXoverDefaultHz[i];
            if (s.readDouble(hz))
                st.xover[i] = std::clamp(hz, Reducer::kXoverMin[i], Reducer::kXoverMax[i]);
        }
        for (auto& on : st.solo)
        {
            int32 v = 0;
            if (s.readInt32(v))
                on = v != 0;
        }
        int32 r = Reducer::kResponseDefault;
        if (s.readInt32(r))
        {
            if (version == 4)
                ++r;               // version 4 had no VERY SLOW in front of SLOW
            st.response = std::clamp(static_cast<int>(r), 0, Reducer::kResponses - 1);
        }
    }
    pending = st;
    engine.setSettings(st);
    return kResultOk;
}

tresult PLUGIN_API WetReducerProcessor::getState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    s.writeInt32(5);
    for (double a : pending.amount)
        s.writeDouble(a);
    for (double hz : pending.xover)
        s.writeDouble(hz);
    for (bool on : pending.solo)
        s.writeInt32(on ? 1 : 0);
    s.writeInt32(pending.response);
    return kResultOk;
}

} // namespace Yonie
