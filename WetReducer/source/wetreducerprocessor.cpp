//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetreducerprocessor.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>

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
    if (id <= kHighParam)
        pending.amount[id] = std::clamp(value, 0.0, 1.0);
    else if (id <= kXover3Param)
        pending.xover[id - kXover1Param] = Reducer::xoverHz(static_cast<int>(id - kXover1Param), value);
    else if (id <= kSolo4Param)
        pending.solo[id - kSolo1Param] = value > 0.5;
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
    return kResultOk;
}

// Prototype state, versioned from the first build: four band amounts and three crossover
// positions.
tresult PLUGIN_API WetReducerProcessor::setState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0;
    if (!s.readInt32(version))
        return kResultFalse;
    ReducerEngine::Settings st;
    for (auto& a : st.amount)
    {
        double v = 0.0;
        if (s.readDouble(v))
            a = std::clamp(v, 0.0, 1.0);
    }
    // Version 2 stores the crossovers in Hz. Version 1 (prototype builds of 2 Oct) stored a
    // choice of three that no longer exists; those sessions get the defaults.
    for (int i = 0; i < 3; ++i)
    {
        double hz = Reducer::kXoverDefaultHz[i];
        if (version >= 2 && s.readDouble(hz))
            st.xover[i] = std::clamp(hz, Reducer::kXoverMin[i], Reducer::kXoverMax[i]);
    }
    if (version >= 3)
    {
        for (auto& on : st.solo)
        {
            int32 v = 0;
            if (s.readInt32(v))
                on = v != 0;
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
    s.writeInt32(3);
    for (double a : pending.amount)
        s.writeDouble(a);
    for (double hz : pending.xover)
        s.writeDouble(hz);
    for (bool on : pending.solo)
        s.writeInt32(on ? 1 : 0);
    return kResultOk;
}

} // namespace Yonie
