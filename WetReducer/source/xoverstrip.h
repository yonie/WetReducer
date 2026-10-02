//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// The band handles on one frequency strip, 30 Hz to 16 kHz on a log scale (Ronald: "why do we not
// put all the sliders on the same grid ... that way they naturally cant go past one another").
// Six handles, all alike: BASS's low edge, the four crossovers, AIR's high edge. A handle stops
// where its neighbour is, so the bands stay in order. The bands are shaded and named between them.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/cview.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/events.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "wetreducercids.h"
#include "reducerengine.h"

namespace Yonie {

class XoverStrip : public VSTGUI::CView
{
public:
    XoverStrip(const VSTGUI::CRect& size, Steinberg::Vst::EditController* controller);
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
    double hzAt(double x) const;
    double xAt(double hz) const;
    VSTGUI::CRect track() const;

    Steinberg::Vst::EditController* controller;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
    double shown[32] = {};          // the parameters as last drawn
    int dragging = -1;
};

} // namespace Yonie
