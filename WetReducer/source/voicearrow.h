//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// The VOICES up and down tip buttons. Each is bound to the voice count and steps it by one per
// click; the rendered cap shows pressed while the mouse is down. The display beside them is a
// CMovieBitmap on the same parameter.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/controls/ccontrol.h"

namespace Yonie {

class VoiceArrow : public VSTGUI::CControl
{
public:
    VoiceArrow(const VSTGUI::CRect& size, VSTGUI::CBitmap* frames, int direction);

    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
    CLASS_METHODS(VoiceArrow, CControl)

private:
    VSTGUI::SharedPointer<VSTGUI::CBitmap> frames;    // two frames stacked vertically: up, pressed
    int direction;                                     // +1 up, -1 down
    bool pressed = false;
};

} // namespace Yonie
