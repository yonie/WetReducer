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
const CColor kBand[4] = {CColor(217, 89, 38), CColor(232, 163, 61), CColor(25, 163, 111), CColor(57, 135, 229)};
const char* kBandName[4] = {"LOW", "LOW MID", "HIGH MID", "HIGH"};
const double kLo = 30.0, kHi = 16000.0;

void noteName(double hz, char* out, size_t n)
{
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int midi = static_cast<int>(std::lround(69.0 + 12.0 * std::log2(hz / 440.0)));
    if (hz < 1000.0)
        std::snprintf(out, n, "%.0f Hz %s%d", hz, names[(midi % 12 + 12) % 12], midi / 12 - 1);
    else
        std::snprintf(out, n, "%.2f kHz %s%d", hz / 1000.0, names[(midi % 12 + 12) % 12], midi / 12 - 1);
}
} // namespace

XoverStrip::XoverStrip(const CRect& size, Steinberg::Vst::EditController* c) : CView(size), controller(c)
{
    // Follow the parameters (host automation, a loaded session) without a listener per tag.
    timer = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) {
        bool changed = false;
        for (int i = 0; i < 7; ++i)
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
    return Reducer::xoverHz(i, controller->getParamNormalized(kXover1Param + i));
}

void XoverStrip::draw(CDrawContext* ctx)
{
    const CRect t = track();
    ctx->setDrawMode(kAntiAliasing);
    ctx->setFillColor(kTrack);
    ctx->drawRect(t, kDrawFilled);

    // the four bands between the handles
    double edge[5] = {kLo, hzOf(0), hzOf(1), hzOf(2), kHi};
    auto font = makeOwned<CFontDesc>("Arial", 11, kBoldFace);
    ctx->setFont(font);
    for (int b = 0; b < 4; ++b)
    {
        CRect band(xAt(edge[b]), t.top, xAt(edge[b + 1]), t.bottom);
        // the band's colour, stronger the further its knob is turned
        CColor c = kBand[b];
        c.alpha = static_cast<uint8_t>(40 + 200 * controller->getParamNormalized(kLowParam + b));
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
    for (int i = 0; i < 3; ++i)
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
    for (int i = 0; i < 3; ++i)
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
    controller->beginEdit(kXover1Param + dragging);
    return onMouseMoved(where, buttons);
}

CMouseEventResult XoverStrip::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    if (dragging < 0 || !buttons.isLeftButton())
        return kMouseEventNotHandled;
    // a handle stops at its neighbours, at least kXoverGap away
    double hz = hzAt(where.x);
    const double below = dragging > 0 ? hzOf(dragging - 1) * Reducer::kXoverGap : kLo;
    const double above = dragging < 2 ? hzOf(dragging + 1) / Reducer::kXoverGap : kHi;
    hz = std::clamp(hz, below, above);
    const auto id = static_cast<Steinberg::Vst::ParamID>(kXover1Param + dragging);
    const auto n = Reducer::xoverNorm(dragging, hz);
    controller->setParamNormalized(id, n);
    controller->performEdit(id, n);
    invalid();
    return kMouseEventHandled;
}

CMouseEventResult XoverStrip::onMouseUp(CPoint& where, const CButtonState& buttons)
{
    if (dragging >= 0)
        controller->endEdit(kXover1Param + dragging);
    dragging = -1;
    invalid();
    return kMouseEventHandled;
}

} // namespace Yonie
