//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "xoverstrip.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;

namespace Yonie {

namespace {
const CColor kInk(40, 38, 34);
const CColor kReadoutBack(244, 240, 228, 230);

void noteLabel(int midi, char* out, size_t n)
{
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const double hz = Reducer::noteHz(midi);
    if (hz < 1000.0)
        std::snprintf(out, n, "%s%d  %.0f Hz", names[(midi % 12 + 12) % 12], midi / 12 - 1, hz);
    else
        std::snprintf(out, n, "%s%d  %.2f kHz", names[(midi % 12 + 12) % 12], midi / 12 - 1, hz / 1000.0);
}
} // namespace

XoverStrip::XoverStrip(const CRect& size, Steinberg::Vst::EditController* c, const StripGeometry& geometry)
: CView(size), controller(c), g(geometry)
{
    setTransparency(true);
    // Follow the parameters (host automation, a loaded session) without a listener per tag, and
    // let the note readout go a moment after a cap stops moving.
    timer = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) {
        bool changed = false;
        for (int h = 0; h < kHandles; ++h)
        {
            const auto id = paramOf(h);
            const double v = controller->getParamNormalized(id);
            if (std::abs(v - shown[id]) > 1e-6)
            {
                shown[id] = v;
                changed = true;
            }
        }
        if (readout >= 0 && dragging < 0 && --readoutTicks <= 0)
        {
            readout = -1;
            changed = true;
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

// Six handles on one grid, left to right: BASS's low edge, the four crossovers, AIR's high edge.
Steinberg::Vst::ParamID XoverStrip::paramOf(int handle)
{
    if (handle == 0)
        return kLowEdgeParam;
    if (handle == kHandles - 1)
        return kHighEdgeParam;
    return static_cast<Steinberg::Vst::ParamID>(kXoverParam + handle - 1);
}

int XoverStrip::noteOf(int handle) const
{
    return Reducer::xoverNote(controller->getParamNormalized(paramOf(handle)));
}

double XoverStrip::xOfNote(double note) const
{
    return g.trackX0 + (g.trackX1 - g.trackX0) * (note - Reducer::kXoverLowNote) / Reducer::kXoverNotes;
}

int XoverStrip::noteAt(double x) const
{
    const double f = (x - g.trackX0) / (g.trackX1 - g.trackX0);
    return Reducer::kXoverLowNote + static_cast<int>(std::lround(f * Reducer::kXoverNotes));
}

int XoverStrip::handleAt(const CPoint& where) const
{
    int nearest = -1;
    double best = 1e9;
    for (int h = 0; h < kHandles; ++h)
    {
        const double d = std::abs(where.x - xOfNote(noteOf(h)));
        if (d < best)
        {
            best = d;
            nearest = h;
        }
    }
    return best <= 24.0 ? nearest : -1;
}

void XoverStrip::draw(CDrawContext* ctx)
{
    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);
    double x[kHandles];
    for (int h = 0; h < kHandles; ++h)
        x[h] = xOfNote(noteOf(h));

    // The coloured line under the slot: one band between each pair of caps, nothing outside the
    // outer two, where the music passes as it is.
    for (int b = 0; b < Reducer::kBands; ++b)
    {
        const double x0 = x[b] + 3.0, x1 = x[b + 1] - 3.0;
        if (x1 <= x0)
            continue;
        ctx->setFillColor(g.band[b]);
        ctx->drawRect(CRect(x0, g.lineY - g.lineH / 2.0, x1, g.lineY + g.lineH / 2.0), kDrawFilled);
    }

    // The caps, each a rendered sprite with its own shadow.
    for (int h = 0; h < kHandles; ++h)
    {
        auto& bm = g.cap[h];
        if (!bm)
            continue;
        const CPoint a = g.anchor[h];
        const CRect r(x[h] - a.x, g.capY - a.y, x[h] - a.x + bm->getWidth(), g.capY - a.y + bm->getHeight());
        bm->draw(ctx, r);
    }

    // While a cap moves, its note above it.
    if (readout >= 0)
    {
        char text[32];
        noteLabel(noteOf(readout), text, sizeof(text));
        auto font = makeOwned<CFontDesc>("Arial", 11, kBoldFace);
        ctx->setFont(font);
        const double w = ctx->getStringWidth(text) + 12.0;
        const double top = getViewSize().top + 1.0;
        CRect r(x[readout] - w / 2.0, top, x[readout] + w / 2.0, top + 17.0);
        if (r.left < getViewSize().left)
            r.offset(getViewSize().left - r.left, 0);
        if (r.right > getViewSize().right)
            r.offset(getViewSize().right - r.right, 0);
        ctx->setFillColor(kReadoutBack);
        ctx->drawRect(r, kDrawFilled);
        ctx->setFontColor(kInk);
        ctx->drawString(text, r, kCenterText);
    }
    setDirty(false);
}

CMouseEventResult XoverStrip::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (!buttons.isLeftButton())
        return kMouseEventNotHandled;
    dragging = handleAt(where);
    if (dragging < 0)
        return kMouseEventHandled;
    readout = dragging;
    controller->beginEdit(paramOf(dragging));
    return onMouseMoved(where, buttons);
}

CMouseEventResult XoverStrip::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    if (dragging < 0 || !buttons.isLeftButton())
        return kMouseEventNotHandled;
    moveTo(dragging, noteAt(where.x));
    return kMouseEventHandled;
}

CMouseEventResult XoverStrip::onMouseUp(CPoint& where, const CButtonState& buttons)
{
    if (dragging >= 0)
        controller->endEdit(paramOf(dragging));
    dragging = -1;
    readoutTicks = 30;          // about a second, then the readout goes
    invalid();
    return kMouseEventHandled;
}

// One notch of the wheel moves the cap nearest the pointer one semitone; up is higher.
void XoverStrip::onMouseWheelEvent(MouseWheelEvent& event)
{
    const int step = event.deltaY > 0.0 ? 1 : event.deltaY < 0.0 ? -1 : 0;
    if (step == 0)
        return;
    int nearest = 0;
    double best = 1e9;
    for (int h = 0; h < kHandles; ++h)
    {
        const double d = std::abs(event.mousePosition.x - xOfNote(noteOf(h)));
        if (d < best)
        {
            best = d;
            nearest = h;
        }
    }
    controller->beginEdit(paramOf(nearest));
    moveTo(nearest, noteOf(nearest) + step);
    controller->endEdit(paramOf(nearest));
    readout = nearest;
    readoutTicks = 30;
    event.consumed = true;
}

void XoverStrip::moveTo(int handle, int note)
{
    const int lowest = Reducer::kXoverLowNote, highest = Reducer::kXoverLowNote + Reducer::kXoverNotes;
    const int below = handle > 0 ? noteOf(handle - 1) + Reducer::kXoverGapNotes : lowest;
    const int above = handle < kHandles - 1 ? noteOf(handle + 1) - Reducer::kXoverGapNotes : highest;
    note = std::clamp(note, below, above);
    const auto id = paramOf(handle);
    const auto n = Reducer::xoverNorm(0, Reducer::noteHz(note));
    controller->setParamNormalized(id, n);
    controller->performEdit(id, n);
    invalid();
}

} // namespace Yonie
