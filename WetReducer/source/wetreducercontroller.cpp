//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetreducercontroller.h"
#include "wetreducercids.h"
#include "reducerengine.h"
#include "weteditor.h"
#include "xoverstrip.h"
#include "steppedknob.h"
#include "bandled.h"
#include "voicearrow.h"
#include "customviewcreator.h"
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
        info.stepCount = Reducer::kXoverNotes;      // one step per semitone
        info.defaultNormalizedValue = Reducer::xoverNorm(index, Reducer::kXoverDefaultHz[index]);
        info.flags = Vst::ParameterInfo::kCanAutomate;
        setNormalized(info.defaultNormalizedValue);
    }
    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        static const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const int midi = Reducer::xoverNote(normalized);
        const double hz = Reducer::noteHz(midi);
        char text[64];
        if (hz < 1000.0)
            std::snprintf(text, sizeof(text), "%s%d  %.0f Hz", notes[midi % 12], midi / 12 - 1, hz);
        else
            std::snprintf(text, sizeof(text), "%s%d  %.2f kHz", notes[midi % 12], midi / 12 - 1, hz / 1000.0);
        UString(string, str16BufferSize(Vst::String128)).fromAscii(text);
    }
    Vst::ParamValue toPlain(Vst::ParamValue normalized) const SMTG_OVERRIDE { return Reducer::xoverHz(index, normalized); }
    Vst::ParamValue toNormalized(Vst::ParamValue plain) const SMTG_OVERRIDE { return Reducer::xoverNorm(index, plain); }

private:
    int index;
};
// A band edge on the crossovers' note grid, "Off" at its own end of the strip.
class FilterParameter : public XoverParameter
{
public:
    FilterParameter(const Vst::TChar* title, Vst::ParamID id, bool high) : XoverParameter(title, id, 0), high(high)
    {
        info.defaultNormalizedValue =
            Reducer::xoverNorm(0, Reducer::noteHz(high ? Reducer::kHighEdgeDefaultNote : Reducer::kLowEdgeDefaultNote));
        setNormalized(info.defaultNormalizedValue);
    }
    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        const double hz = Reducer::xoverHz(0, normalized);
        if (high ? !Reducer::highEdgeOn(hz) : !Reducer::lowEdgeOn(hz))
            UString(string, str16BufferSize(Vst::String128)).fromAscii("Off");
        else
            XoverParameter::toString(normalized, string);
    }

private:
    bool high;
};
// A band amount, 0..100 in 41 steps, printed as the number the panel shows.
class BandParameter : public Vst::Parameter
{
public:
    BandParameter(const Vst::TChar* title, Vst::ParamID id)
    {
        UString(info.title, str16BufferSize(Vst::String128)).assign(title);
        UString(info.units, str16BufferSize(Vst::String128)).assign(STR16("%"));
        info.id = id;
        info.stepCount = 40;
        info.defaultNormalizedValue = 0.0;
        info.flags = Vst::ParameterInfo::kCanAutomate;
    }
    void toString(Vst::ParamValue normalized, Vst::String128 string) const SMTG_OVERRIDE
    {
        char text[32];
        const double v = std::round(normalized * 40.0) * 2.5;
        if (v == std::floor(v))
            std::snprintf(text, sizeof(text), "%.0f", v);
        else
            std::snprintf(text, sizeof(text), "%.1f", v);
        UString(string, str16BufferSize(Vst::String128)).fromAscii(text);
    }
    Vst::ParamValue toPlain(Vst::ParamValue normalized) const SMTG_OVERRIDE { return normalized * 100.0; }
    Vst::ParamValue toNormalized(Vst::ParamValue plain) const SMTG_OVERRIDE { return plain / 100.0; }
};
} // namespace

tresult PLUGIN_API WetReducerController::initialize(FUnknown* context)
{
    tresult result = EditControllerEx1::initialize(context);
    registerCustomViews();
    if (result != kResultOk)
        return result;

    const Vst::TChar* bands[] = {STR16("Bass"), STR16("Body"), STR16("Voice"), STR16("Presence"), STR16("Air")};
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        // 41 positions, 2.5 apart: the printed detents are every 10, Shift reaches the rest
        parameters.addParameter(new BandParameter(bands[b], kBandParam + b));
    }

    const Vst::TChar* names[] = {STR16("Bass | Body"), STR16("Body | Voice"), STR16("Voice | Presence"), STR16("Presence | Air")};
    for (int x = 0; x < Reducer::kXovers; ++x)
        parameters.addParameter(new XoverParameter(names[x], kXoverParam + x, x));

    const Vst::TChar* solos[] = {STR16("Solo Bass"), STR16("Solo Body"), STR16("Solo Voice"), STR16("Solo Presence"), STR16("Solo Air")};
    for (int b = 0; b < Reducer::kBands; ++b)
        parameters.addParameter(solos[b], nullptr, 1, 0.0, Vst::ParameterInfo::kCanAutomate, kSoloParam + b);

    // The lamps, fed by the processor. Read-only so a host never offers to automate them.
    const Vst::TChar* leds[] = {STR16("Lamp Bass"), STR16("Lamp Body"), STR16("Lamp Voice"), STR16("Lamp Presence"), STR16("Lamp Air")};
    for (int b = 0; b < Reducer::kBands; ++b)
        parameters.addParameter(leds[b], nullptr, 0, 0.0, Vst::ParameterInfo::kIsReadOnly, kLedParam + b);

    parameters.addParameter(new FilterParameter(STR16("Bass from"), kLowEdgeParam, false));
    parameters.addParameter(new FilterParameter(STR16("Air up to"), kHighEdgeParam, true));

    // VOICES: tones per band, 1..8, stepped by the display's up and down buttons
    parameters.addParameter(STR16("Voices"), nullptr, Reducer::kVoices - 1,
                            Reducer::voicesNorm(Reducer::kVoicesDefault), Vst::ParameterInfo::kCanAutomate,
                            kVoicesParam);

    auto* response = new Vst::StringListParameter(STR16("Response"), kResponseParam, nullptr,
                                                  Vst::ParameterInfo::kCanAutomate | Vst::ParameterInfo::kIsList);
    response->appendString(STR16("Slow"));
    response->appendString(STR16("Fast"));
    response->getInfo().defaultNormalizedValue = Reducer::responseNorm(Reducer::kResponseDefault);
    response->setNormalized(Reducer::responseNorm(Reducer::kResponseDefault));
    parameters.addParameter(response);
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
    if (pushing || tag < kXoverParam || tag >= kXoverParam + Reducer::kXovers)
        return EditControllerEx1::performEdit(tag, value);

    // in semitones: neighbours stay at least kXoverGapNotes apart
    const int moved = static_cast<int>(tag - kXoverParam);
    int note[Reducer::kXovers];
    for (int i = 0; i < Reducer::kXovers; ++i)
        note[i] = Reducer::xoverNote(getParamNormalized(kXoverParam + i));
    const int lowest = Reducer::kXoverLowNote, highest = Reducer::kXoverLowNote + Reducer::kXoverNotes;
    note[moved] = std::clamp(Reducer::xoverNote(value), lowest + moved * Reducer::kXoverGapNotes,
                             highest - (Reducer::kXovers - 1 - moved) * Reducer::kXoverGapNotes);
    for (int i = moved + 1; i < Reducer::kXovers; ++i)
        note[i] = std::max(note[i], note[i - 1] + Reducer::kXoverGapNotes);
    for (int i = moved - 1; i >= 0; --i)
        note[i] = std::min(note[i], note[i + 1] - Reducer::kXoverGapNotes);
    double hz[Reducer::kXovers];
    for (int i = 0; i < Reducer::kXovers; ++i)
        hz[i] = Reducer::noteHz(note[i]);

    const Vst::ParamValue own = Reducer::xoverNorm(moved, hz[moved]);
    setParamNormalized(tag, own);
    tresult r = EditControllerEx1::performEdit(tag, own);
    pushing = true;
    for (int i = 0; i < Reducer::kXovers; ++i)
    {
        if (i == moved)
            continue;
        const Vst::ParamID id = kXoverParam + i;
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
                                                     const VSTGUI::IUIDescription* description, VSTGUI::VST3Editor* /*editor*/)
{
    if (!name)
        return nullptr;
    const std::string n(name);
    VSTGUI::CPoint origin, size;
    attributes.getPointAttribute("origin", origin);
    attributes.getPointAttribute("size", size);
    const VSTGUI::CRect rect(origin, size);
    auto bitmap = [&](const char* attr) -> VSTGUI::CBitmap* {
        auto* v = attributes.getAttributeValue(attr);
        return v ? description->getBitmap(v->data()) : nullptr;
    };
    auto number = [&](const char* attr, double fallback) {
        double d = fallback;
        attributes.getDoubleAttribute(attr, d);
        return d;
    };

    if (n == "BandLed")
        return new BandLed(rect, bitmap("bitmap"));
    if (n == "VoiceArrow")
        return new VoiceArrow(rect, bitmap("bitmap"), static_cast<int>(number("direction", 1.0)));
    if (n == "XoverStrip")
    {
        // All geometry measured off the render, written into the .uidesc by
        // tools/sync-reducer-uidesc.py from the asset script's layout.json.
        StripGeometry g;
        g.trackX0 = number("track-x0", rect.left);
        g.trackX1 = number("track-x1", rect.right);
        g.lineY = number("line-y", rect.bottom - 4);
        g.lineH = number("line-h", 6);
        g.capY = number("cap-y", rect.getCenter().y);
        if (auto* v = attributes.getAttributeValue("band-colors"))
        {
            std::string list = *v;
            for (int b = 0, start = 0; b < Reducer::kBands; ++b)
            {
                const auto comma = list.find(',', start);
                const std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
                unsigned r = 0, gr = 0, bl = 0;
                if (std::sscanf(item.c_str(), "#%02x%02x%02x", &r, &gr, &bl) == 3)
                    g.band[b] = VSTGUI::CColor(static_cast<uint8_t>(r), static_cast<uint8_t>(gr), static_cast<uint8_t>(bl));
                if (comma == std::string::npos)
                    break;
                start = static_cast<int>(comma) + 1;
            }
        }
        for (int h = 0; h <= Reducer::kBands; ++h)
        {
            char key[32];
            std::snprintf(key, sizeof(key), "cap-%d", h);
            g.cap[h] = bitmap(key);
            std::snprintf(key, sizeof(key), "cap-%d-anchor", h);
            VSTGUI::CPoint a;
            if (attributes.getPointAttribute(key, a))
                g.anchor[h] = a;
        }
        return new XoverStrip(rect, this, g);
    }
    return nullptr;
}

// The processor's saved state, version 1 (see wetreducerprocessor.cpp for the layout).
tresult PLUGIN_API WetReducerController::setComponentState(IBStream* state)
{
    if (!state)
        return kResultFalse;
    IBStreamer s(state, kLittleEndian);
    int32 version = 0;
    if (!s.readInt32(version))
        return kResultFalse;
    if (version != 1)
        return kResultOk;
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        double v = 0.0;
        if (s.readDouble(v))
            setParamNormalized(kBandParam + b, v);
    }
    for (int x = 0; x < Reducer::kXovers; ++x)
    {
        double hz = Reducer::kXoverDefaultHz[x];
        s.readDouble(hz);
        setParamNormalized(kXoverParam + x, Reducer::xoverNorm(x, hz));
    }
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        int32 v = 0;
        if (s.readInt32(v))
            setParamNormalized(kSoloParam + b, v ? 1.0 : 0.0);
    }
    int32 r = Reducer::kResponseDefault;
    if (s.readInt32(r))
        setParamNormalized(kResponseParam, Reducer::responseNorm(std::clamp(static_cast<int>(r), 0, Reducer::kResponses - 1)));
    double hz = 0.0;
    if (s.readDouble(hz))
        setParamNormalized(kLowEdgeParam, Reducer::xoverNorm(0, hz));
    if (s.readDouble(hz))
        setParamNormalized(kHighEdgeParam, Reducer::xoverNorm(0, hz));
    int32 n = Reducer::kVoicesDefault;
    if (s.readInt32(n))
        setParamNormalized(kVoicesParam, Reducer::voicesNorm(n));
    return kResultOk;
}

} // namespace Yonie
