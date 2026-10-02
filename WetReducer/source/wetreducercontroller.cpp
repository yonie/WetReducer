//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetreducercontroller.h"
#include "wetreducercids.h"
#include "reducerengine.h"
#include "weteditor.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/base/ustring.h"

#include <cmath>
#include <cstdio>

using namespace Steinberg;

namespace Yonie {

namespace {
// A crossover on a log scale, printed in Hz with the nearest note.
class XoverParameter : public Vst::Parameter
{
public:
    XoverParameter(const Vst::TChar* title, Vst::ParamID id, int index) : index(index)
    {
        UString(info.title, str16BufferSize(Vst::String128)).assign(title);
        info.id = id;
        info.stepCount = 0;
        info.defaultNormalizedValue = Reducer::xoverNorm(index, Reducer::kXoverDefaultHz[index]);
        info.flags = Vst::ParameterInfo::kCanAutomate;
        setNormalized(info.defaultNormalizedValue);
    }
    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        static const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const double hz = Reducer::xoverHz(index, normalized);
        const int midi = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(hz / 440.0)));
        char text[64];
        if (hz < 1000.0)
            std::snprintf(text, sizeof(text), "%.0f Hz  %s%d", hz, notes[(midi % 12 + 12) % 12], midi / 12 - 1);
        else
            std::snprintf(text, sizeof(text), "%.2f kHz  %s%d", hz / 1000.0, notes[(midi % 12 + 12) % 12], midi / 12 - 1);
        UString(string, str16BufferSize(Vst::String128)).fromAscii(text);
    }
    Vst::ParamValue toPlain(Vst::ParamValue normalized) const SMTG_OVERRIDE { return Reducer::xoverHz(index, normalized); }
    Vst::ParamValue toNormalized(Vst::ParamValue plain) const SMTG_OVERRIDE { return Reducer::xoverNorm(index, plain); }

private:
    int index;
};
} // namespace

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
    for (int x = 0; x < 3; ++x)
        parameters.addParameter(new XoverParameter(names[x], kXover1Param + x, x));
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
        double hz = Reducer::kXoverDefaultHz[x];
        if (version >= 2)
            s.readDouble(hz);
        setParamNormalized(kXover1Param + x, Reducer::xoverNorm(x, hz));
    }
    return kResultOk;
}

} // namespace Yonie
