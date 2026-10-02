//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetreducercontroller.h"
#include "wetreducercids.h"
#include "reducerengine.h"
#include "weteditor.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/vstparameters.h"

using namespace Steinberg;

namespace Yonie {

tresult PLUGIN_API WetReducerController::initialize(FUnknown* context)
{
    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk)
        return result;

    const Vst::TChar* bands[] = {STR16("Low"), STR16("Low Mid"), STR16("High Mid"), STR16("High")};
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        auto* p = new Vst::RangeParameter(bands[b], kLowParam + b, STR16("%"), 0.0, 100.0, 0.0, 0,
                                          Vst::ParameterInfo::kCanAutomate);
        p->setPrecision(0);
        parameters.addParameter(p);
    }

    const Vst::TChar* names[] = {STR16("LF / LMF"), STR16("LMF / HMF"), STR16("HMF / HF")};
    const Vst::TChar* choices[3][3] = {{STR16("50 Hz"), STR16("70 Hz"), STR16("90 Hz")},
                                       {STR16("1 kHz"), STR16("3 kHz"), STR16("5 kHz")},
                                       {STR16("5 kHz"), STR16("8 kHz"), STR16("10 kHz")}};
    for (int x = 0; x < 3; ++x)
    {
        auto* p = new Vst::StringListParameter(names[x], kXover1Param + x, nullptr, Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsList);
        for (auto* c : choices[x])
            p->appendString(c);
        p->setNormalized(Reducer::kXoverDefault / 2.0);
        p->getInfo().defaultNormalizedValue = Reducer::kXoverDefault / 2.0;
        parameters.addParameter(p);
    }
    return kResultOk;
}

IPlugView* PLUGIN_API WetReducerController::createView(FIDString name)
{
    if (FIDStringsEqual(name, Vst::ViewType::kEditor))
    {
        auto* editor = new Yonie::WetEditor(this, "view", "wetreducereditor.uidesc");
        editor->setAllowedZoomFactors({0.75, 1.0, 1.25});
        return editor;
    }
    return nullptr;
}

tresult PLUGIN_API WetReducerController::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0;
    if (!s.readInt32(version))
        return kResultFalse;
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        double v = 0.0;
        if (s.readDouble(v))
            setParamNormalized(kLowParam + b, v);
    }
    for (int x = 0; x < 3; ++x)
    {
        int32 v = Reducer::kXoverDefault;
        if (s.readInt32(v))
            setParamNormalized(kXover1Param + x, v / 2.0);
    }
    return kResultOk;
}

} // namespace Yonie
