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

tresult PLUGIN_API WetReducerProcessor::setBusArrangements(Vst::SpeakerArrangement* inputs, int32 numIns,
                                                          Vst::SpeakerArrangement* outputs, int32 numOuts)
{
    // All four mono/stereo layouts (settled decision 24); monobus.h turns mono into stereo.
    if (!Wet::MonoBus::accepts(inputs, numIns, outputs, numOuts))
        return kResultFalse;
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

tresult PLUGIN_API WetReducerProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
{
    engine.prepare(newSetup.sampleRate);
    engine.setSettings(pending);
    monoBus.prepare(newSetup.maxSamplesPerBlock);
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
    else if (id == kLowEdgeParam)
        pending.lowEdge = Reducer::xoverHz(0, value);
    else if (id == kHighEdgeParam)
        pending.highEdge = Reducer::xoverHz(0, value);
    else if (id == kVoicesParam)
        pending.voices = Reducer::voicesOf(value);

}

// The lamps blink: on for 60 ms each time a band's tone moves to a new note, then off - no
// fade, no glow (Ronald: "why does it not just blink whenever it picks a new note"). A blink
// is followed by at least 40 ms dark, so notes arriving faster still read as blinks. Brighter
// the further the band's knob is turned; dark at 0 or while another band's solo silences it.
void WetReducerProcessor::sendLeds(Vst::ProcessData& data)
{
    const auto& st = engine.settings();
    bool anySolo = false;
    for (bool on : st.solo)
        anySolo = anySolo || on;
    const double fs = std::max(1.0, processSetup.sampleRate);
    const int onSamples = static_cast<int>(0.060 * fs), offSamples = static_cast<int>(0.040 * fs);
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        const double weight = (anySolo && !st.solo[b]) ? 0.0 : st.amount[b];
        const int changes = engine.bandNoteChanges(b);
        const bool newNote = changes != seenChanges[b];
        seenChanges[b] = changes;
        const bool heard = engine.bandLevel(b) > 0.001;          // about -60 dBFS
        blinkLeft[b] = std::max(blinkLeft[b] - data.numSamples, -offSamples - 1);
        if (newNote && heard && blinkLeft[b] <= -offSamples)
            blinkLeft[b] = onSamples;
        led[b] = blinkLeft[b] > 0 ? weight : 0.0;
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

    // Mono buses become stereo here: see monobus.h.
    float *inL, *inR, *outL, *outR;
    if (!monoBus.begin(data.inputs[0], data.outputs[0], data.numSamples, inL, inR, outL, outR))
        return kResultOk;
    engine.process(inL, inR, outL, outR, data.numSamples);
    monoBus.end(data.outputs[0], data.numSamples, outL, outR);
    data.outputs[0].silenceFlags = 0;
    sendLeds(data);
    return kResultOk;
}
// Saved state, version 1 - the format the first release ships with (prototype formats are gone,
// AGENTS.md). Every field says what it means rather than which step it was on, so a later change
// to a control's grid cannot silently move a saved setting (hub AGENTS.md, settled decision 16):
//   int32  version (1)
//   double amount[5]      band knob position 0..1 (0 the original, 0.5 all sine, 1 all square)
//   double xover[4]       the four splits, in Hz
//   int32  solo[5]        0 / 1
//   int32  response       0 SLOW, 1 FAST
//   double lowEdge        where BASS starts, Hz (at the bottom of the grid: off)
//   double highEdge       where AIR ends, Hz (at the top of the grid: off)
//   int32  voices         1..8
tresult PLUGIN_API WetReducerProcessor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0;
    if (!s.readInt32(version))
        return kResultFalse;
    ReducerEngine::Settings st;
    if (version == 1)
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
            st.response = std::clamp(static_cast<int>(r), 0, Reducer::kResponses - 1);
        double hz = 0.0;
        if (s.readDouble(hz))
            st.lowEdge = std::clamp(hz, Reducer::edgeBottomHz(), Reducer::edgeTopHz());
        if (s.readDouble(hz))
            st.highEdge = std::clamp(hz, Reducer::edgeBottomHz(), Reducer::edgeTopHz());
        int32 n = Reducer::kVoicesDefault;
        if (s.readInt32(n))
            st.voices = std::clamp(static_cast<int>(n), 1, Reducer::kVoices);
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
    s.writeInt32(1);
    for (double a : pending.amount)
        s.writeDouble(a);
    for (double hz : pending.xover)
        s.writeDouble(hz);
    for (bool on : pending.solo)
        s.writeInt32(on ? 1 : 0);
    s.writeInt32(pending.response);
    s.writeDouble(pending.lowEdge);
    s.writeDouble(pending.highEdge);
    s.writeInt32(pending.voices);
    return kResultOk;
}

} // namespace Yonie
