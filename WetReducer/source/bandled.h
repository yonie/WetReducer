//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// One lamp per band, in the band's colour. Fed by a read-only parameter from the processor:
// it flashes each time the band's tone moves to a new note, brighter the further its knob is
// turned (Ronald, 2026-10-02: "lights up whenever it catches and more intensely when its set
// to be heard more intense").
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/controls/ccontrol.h"

namespace Yonie {

class BandLed : public VSTGUI::CControl
{
public:
    BandLed(const VSTGUI::CRect& size, const VSTGUI::CColor& colour);

    void draw(VSTGUI::CDrawContext* context) override;
    CLASS_METHODS(BandLed, CControl)

private:
    VSTGUI::CColor colour;
};

} // namespace Yonie
