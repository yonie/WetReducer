//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// The frequency strip: one fader slot with six caps on a semitone grid - BASS's low edge, the
// four crossovers, AIR's high edge (Ronald: "why do we not put all the sliders on the same grid
// ... that way they naturally cant go past one another"). A cap stops where its neighbour is, so
// the bands stay in order.
//
// The slot, its octave marks and note names are in the rendered backplate. What moves is drawn
// here: the coloured line under the slot between the caps, and the caps themselves - rendered
// sprites carrying their own shadow. All geometry comes from the asset measurements (layout.json,
// via the generated .uidesc), in editor pixels.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/events.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "wetreducercids.h"
#include "reducerengine.h"

namespace Yonie {

struct StripGeometry
{
    double trackX0 = 0, trackX1 = 1;    // where note kXoverLowNote and the top note sit
    double lineY = 0, lineH = 6;        // the coloured line under the slot (centre, thickness)
    double lineInset = 3;               // how far each band's stretch stops short of a cap's centre
    double capY = 0;                    // the caps' centre line
    VSTGUI::CColor band[Reducer::kBands];
    VSTGUI::SharedPointer<VSTGUI::CBitmap> cap[Reducer::kBands + 1];
    VSTGUI::CPoint anchor[Reducer::kBands + 1];     // the pixel in each sprite on the cap's centre
};

class XoverStrip : public VSTGUI::CView
{
public:
    XoverStrip(const VSTGUI::CRect& size, Steinberg::Vst::EditController* controller, const StripGeometry& geometry);
    ~XoverStrip() override;

    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
    void onMouseWheelEvent(VSTGUI::MouseWheelEvent& event) override;

private:
    static constexpr int kHandles = Reducer::kBands + 1;    // BASS's low edge, four crossovers, AIR's high edge
    static Steinberg::Vst::ParamID paramOf(int handle);
    int noteOf(int handle) const;
    void moveTo(int handle, int note);      // clamped to the neighbours, then sent to the host
    double xOfNote(double note) const;
    int noteAt(double x) const;
    int handleAt(const VSTGUI::CPoint& where) const;

    Steinberg::Vst::EditController* controller;
    StripGeometry g;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
    double shown[32] = {};          // the parameters as last drawn
    int dragging = -1;
    int readout = -1;               // the handle whose note is shown, while it moves
    int readoutTicks = 0;
};

} // namespace Yonie
