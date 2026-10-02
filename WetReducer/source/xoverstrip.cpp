//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "xoverstrip.h"
#include "reducerengine.h"
#include "wetreducercids.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cgraphicspath.h"

#include <cmath>
#include <cstdio>

using namespace VSTGUI;

namespace Yonie {

namespace {
const CColor kTrack(44, 46, 51);
const CColor kRule(58, 60, 66);
const CColor kInk(217, 214, 207);
const CColor kDim(138, 135, 128);
const CColor kHandle(95, 179, 201);
// One colour per band, the same as its knob on the panel.
// One colour per band, the same as its knob; checked for colour-blind and normal-vision
// separation on the panel's dark plate (dataviz validator, 2026-10-02).
const CColor kBand[5] = {CColor(0xd9, 0x59, 0x26), CColor(0x90, 0x85, 0xe9), CColor(0xc9, 0x85, 0x00),
                         CColor(0x19, 0x9e, 0x70), CColor(0x39, 0x87, 0xe5)};
const char* kBandName[5] = {"BASS", "BODY", "VOICE", "PRESENCE", "AIR"};
const double kLo = 30.0, kHi = 16000.0;      // the strip's scale; handles live on B0..B9

void noteName(double hz, char* out, size_t n)
{
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int midi = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(hz / 440.0)));
    if (hz < 1000.0)
        std::snprintf(out, n, "%s%d  %.0f Hz", names[(midi % 12 + 12) % 12], midi / 12 - 1, hz);
    else
        std::snprintf(out, n, "%s%d  %.2f kHz", names[(midi % 12 + 12) % 12], midi / 12 - 1, hz / 1000.0);
}
} // namespace

XoverStrip::XoverStrip(const CRect& size, Steinberg::Vst::EditController* c) : CView(size), controller(c)
{
    // Follow the parameters (host automation, a loaded session) without a listener per tag.
    timer = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) {
        bool changed = false;
        for (int i = 0; i < kSoloParam; ++i)
        {
            const double v = controller->getParamNormalized(static_cast<Steinberg::Vst::ParamID>(i));
            if (std::abs(v - shown[i]) > 1e-6)
            {
                shown[i] = v;
                changed = true;
            }
        }
        if (changed)
            invalid();
    }, 30);
}

XoverStrip::~XoverStrip()
{
    if (timer)
        timer->stop();
}

CRect XoverStrip::track() const
{
    CRect r = getViewSize();
    r.top += 22;           // handle readouts above
    r.bottom -= 18;        // frequency scale below
    return r;
}

double XoverStrip::hzAt(double x) const
{
    const CRect t = track();
    const double f = std::clamp((x - t.left) / t.getWidth(), 0.0, 1.0);
    return kLo * std::pow(kHi / kLo, f);
}

double XoverStrip::xAt(double hz) const
{
    const CRect t = track();
    return t.left + t.getWidth() * std::log(std::clamp(hz, kLo, kHi) / kLo) / std::log(kHi / kLo);
}

double XoverStrip::hzOf(int i) const
{
    return Reducer::xoverHz(i, controller->getParamNormalized(kXoverParam + i));
}

void XoverStrip::draw(CDrawContext* ctx)
{
    const CRect t = track();
    ctx->setDrawMode(kAntiAliasing);
    ctx->setFillColor(kTrack);
    ctx->drawRect(t, kDrawFilled);

    // the four bands between the handles
    double edge[Reducer::kBands + 1];
    edge[0] = kLo;
    for (int i = 0; i < Reducer::kXovers; ++i)
        edge[i + 1] = hzOf(i);
    edge[Reducer::kBands] = kHi;
    auto font = makeOwned<CFontDesc>("Arial", 11, kBoldFace);
    ctx->setFont(font);
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        CRect band(xAt(edge[b]), t.top, xAt(edge[b + 1]), t.bottom);
        // the band's colour, stronger the further its knob is turned
        CColor c = kBand[b];
        c.alpha = static_cast<uint8_t>(40 + 200 * controller->getParamNormalized(kBandParam + b));
        ctx->setFillColor(c);
        ctx->drawRect(band, kDrawFilled);
        ctx->setFontColor(kInk);
        if (band.getWidth() > 50)
            ctx->drawString(kBandName[b], band, kCenterText);
    }

    // scale
    auto small = makeOwned<CFontDesc>("Arial", 10);
    ctx->setFont(small);
    ctx->setFontColor(kDim);
    ctx->setFrameColor(kRule);
    ctx->setLineWidth(1);
    const double ticks[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
    const char* tickName[] = {"50", "100", "200", "500", "1k", "2k", "5k", "10k"};
    for (int i = 0; i < 8; ++i)
    {
        const double x = xAt(ticks[i]);
        ctx->drawLine(CPoint(x, t.bottom), CPoint(x, t.bottom + 4));
        ctx->drawString(tickName[i], CRect(x - 20, t.bottom + 4, x + 20, t.bottom + 18), kCenterText);
    }
    ctx->drawString("30 Hz", CRect(t.left, t.bottom + 4, t.left + 40, t.bottom + 18), kLeftText);
    ctx->drawString("16 kHz", CRect(t.right - 40, t.bottom + 4, t.right, t.bottom + 18), kRightText);

    // handles with their readouts
    for (int i = 0; i < Reducer::kXovers; ++i)
    {
        const double x = xAt(edge[i + 1]);
        ctx->setFrameColor(kHandle);
        ctx->setLineWidth(i == dragging ? 4 : 3);
        ctx->drawLine(CPoint(x, t.top - 4), CPoint(x, t.bottom));
        ctx->setFillColor(kHandle);
        ctx->drawRect(CRect(x - 6, t.top - 8, x + 6, t.top), kDrawFilled);
        char text[32];
        noteName(edge[i + 1], text, sizeof(text));
        ctx->setFont(small);
        ctx->setFontColor(kHandle);
        ctx->drawString(text, CRect(x - 60, getViewSize().top, x + 60, t.top - 8), kCenterText);
    }
    setDirty(false);
}

CMouseEventResult XoverStrip::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (!buttons.isLeftButton())
        return kMouseEventNotHandled;
    // the nearest handle takes the drag
    double best = 1e9;
    dragging = -1;
    for (int i = 0; i < Reducer::kXovers; ++i)
    {
        const double d = std::abs(where.x - xAt(hzOf(i)));
        if (d < best)
        {
            best = d;
            dragging = i;
        }
    }
    if (best > 30)
    {
        dragging = -1;
        return kMouseEventHandled;
    }
    controller->beginEdit(kXoverParam + dragging);
    return onMouseMoved(where, buttons);
}

CMouseEventResult XoverStrip::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    if (dragging < 0 || !buttons.isLeftButton())
        return kMouseEventNotHandled;
    // a handle moves in semitones and stops at its neighbours, kXoverGapNotes away
    auto noteOfHz = [](double hz) { return static_cast<int>(std::lround(69.0 + 12.0 * std::log2(hz / 440.0))); };
    int note = noteOfHz(hzAt(where.x));
    const int below = dragging > 0 ? noteOfHz(hzOf(dragging - 1)) + Reducer::kXoverGapNotes : Reducer::kXoverLowNote;
    const int above = dragging < Reducer::kXovers - 1 ? noteOfHz(hzOf(dragging + 1)) - Reducer::kXoverGapNotes
                                                      : Reducer::kXoverLowNote + Reducer::kXoverNotes;
    note = std::clamp(note, below, above);
    const auto id = static_cast<Steinberg::Vst::ParamID>(kXoverParam + dragging);
    const auto n = Reducer::xoverNorm(dragging, Reducer::noteHz(note));
    controller->setParamNormalized(id, n);
    controller->performEdit(id, n);
    invalid();
    return kMouseEventHandled;
}

CMouseEventResult XoverStrip::onMouseUp(CPoint& where, const CButtonState& buttons)
{
    if (dragging >= 0)
        controller->endEdit(kXoverParam + dragging);
    dragging = -1;
    invalid();
    return kMouseEventHandled;
}

} // namespace Yonie
