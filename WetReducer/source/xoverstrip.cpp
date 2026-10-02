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
} // namespace

XoverStrip::XoverStrip(const CRect& size, Steinberg::Vst::EditController* c, const StripGeometry& geometry)
: CView(size), controller(c), g(geometry)
{
    setTransparency(true);
    // Follow the parameters (host automation, a loaded session) without a listener per tag.
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
        const double x0 = x[b] + g.lineInset, x1 = x[b + 1] - g.lineInset;
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

    setDirty(false);
}

CMouseEventResult XoverStrip::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (!buttons.isLeftButton())
        return kMouseEventNotHandled;
    // a click that misses every cap is not ours: let it reach whatever is underneath
    dragging = handleAt(where);
    if (dragging < 0)
        return kMouseEventNotHandled;
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
