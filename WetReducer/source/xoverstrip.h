//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// The three crossovers on one frequency strip, 30 Hz to 16 kHz on a log scale (Ronald: "why do
// we not put all the sliders on the same grid ... that way they naturally cant go past one
// another"). Three handles; a handle stops where its neighbour is, so the bands stay in order.
// The four bands are shaded and named between the handles.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/cview.h"
#include "vstgui/lib/cvstguitimer.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "wetreducercids.h"

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

private:
    double hzAt(double x) const;
    double xAt(double hz) const;
    double hzOf(int i) const;
    double filterHz(int which) const;       // 0 highpass, 1 lowpass
    // handles 0..3 are the crossovers, 4 the highpass, 5 the lowpass
    static Steinberg::Vst::ParamID paramOf(int handle)
    {
        return static_cast<Steinberg::Vst::ParamID>(handle < 4 ? kXoverParam + handle : (handle == 4 ? kHpfParam : kLpfParam));
    }
    VSTGUI::CRect track() const;

    Steinberg::Vst::EditController* controller;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
    double shown[32] = {};          // the parameters as last drawn
    int dragging = -1;
};

} // namespace Yonie
