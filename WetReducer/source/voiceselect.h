//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// VOICES: an 80s seven-segment LCD showing how many tones each band plays, with an up and a
// down arrow button beside it (Ronald, 2026-10-02). 1..8, default 3.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/controls/ccontrol.h"

namespace Yonie {

class VoiceSelect : public VSTGUI::CControl
{
public:
    explicit VoiceSelect(const VSTGUI::CRect& size);

    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
    CLASS_METHODS(VoiceSelect, CControl)

private:
    VSTGUI::CRect lcd() const;
    VSTGUI::CRect upButton() const;
    VSTGUI::CRect downButton() const;
    int pressed = 0;                // +1 up, -1 down, 0 none
};

} // namespace Yonie
