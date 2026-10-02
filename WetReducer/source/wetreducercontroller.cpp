//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetreducercontroller.h"
#include "wetreducercids.h"
#include "reducerengine.h"
#include "weteditor.h"
#include "xoverstrip.h"
#include "vstgui/uidescription/uiattributes.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/base/ustring.h"

#include <algorithm>
#include <cmath>
#include <string>
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

// The crossover sliders push each other: drag one into its neighbour and the neighbour moves
// along, at least kXoverGap away, and the move goes to the host like any other edit. A slider
// cannot be dragged so far that a neighbour would be pushed off the end of its range.
tresult WetReducerController::performEdit(Vst::ParamID tag, Vst::ParamValue value)
{
    if (pushing || tag < kXover1Param || tag > kXover3Param)
        return EditControllerEx1::performEdit(tag, value);

    const int moved = static_cast<int>(tag - kXover1Param);
    double hz[3];
    for (int i = 0; i < 3; ++i)
        hz[i] = Reducer::xoverHz(i, getParamNormalized(kXover1Param + i));
    hz[moved] = Reducer::xoverHz(moved, value);
    const double room = std::pow(Reducer::kXoverGap, 2 - moved), floorRoom = std::pow(Reducer::kXoverGap, moved);
    hz[moved] = std::clamp(hz[moved], Reducer::kXoverMin[moved] * floorRoom, Reducer::kXoverMax[moved] / room);
    for (int i = moved + 1; i < 3; ++i)
        hz[i] = std::max(hz[i], hz[i - 1] * Reducer::kXoverGap);
    for (int i = moved - 1; i >= 0; --i)
        hz[i] = std::min(hz[i], hz[i + 1] / Reducer::kXoverGap);

    const Vst::ParamValue own = Reducer::xoverNorm(moved, hz[moved]);
    setParamNormalized(tag, own);
    tresult r = EditControllerEx1::performEdit(tag, own);
    pushing = true;
    for (int i = 0; i < 3; ++i)
    {
        if (i == moved)
            continue;
        const Vst::ParamID id = kXover1Param + i;
        const Vst::ParamValue n = Reducer::xoverNorm(i, hz[i]);
        if (std::abs(n - getParamNormalized(id)) < 1e-9)
            continue;
        beginEdit(id);
        setParamNormalized(id, n);
        EditControllerEx1::performEdit(id, n);
        endEdit(id);
    }
    pushing = false;
    return r;
}

VSTGUI::CView* WetReducerController::createCustomView(VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
                                                     const VSTGUI::IUIDescription* /*description*/, VSTGUI::VST3Editor* /*editor*/)
{
    if (name && std::string(name) == "XoverStrip")
    {
        VSTGUI::CPoint origin, size;
        attributes.getPointAttribute("origin", origin);
        attributes.getPointAttribute("size", size);
        return new XoverStrip(VSTGUI::CRect(origin, size), this);
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
