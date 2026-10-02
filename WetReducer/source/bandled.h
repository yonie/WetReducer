//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// One LED per band, in the band's colour, fed by a read-only parameter from the processor: it
// blinks each time the band's tone moves to a new note, brighter the further its knob is turned.
// The two looks are rendered frames (dark, and lit with its glow); the lit one is laid over the
// dark one at the LED's brightness.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/controls/ccontrol.h"

namespace Yonie {

class BandLed : public VSTGUI::CControl
{
public:
    BandLed(const VSTGUI::CRect& size, VSTGUI::CBitmap* frames);

    void draw(VSTGUI::CDrawContext* context) override;
    CLASS_METHODS(BandLed, CControl)

private:
    VSTGUI::SharedPointer<VSTGUI::CBitmap> frames;    // two frames stacked vertically: dark, lit
};

} // namespace Yonie
