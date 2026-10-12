//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "voicearrow.h"
#include "reducerengine.h"

#include "vstgui/lib/cdrawcontext.h"
#include "stripframes.h"

#include <algorithm>

using namespace VSTGUI;

namespace Yonie {

VoiceArrow::VoiceArrow(const CRect& size, CBitmap* f, int dir) : CControl(size, nullptr, -1), frames(f), direction(dir)
{
    setTransparency(true);
}

void VoiceArrow::draw(CDrawContext* context)
{
    if (frames)
        Strip::draw(context, frames, getViewSize(), pressed ? 1 : 0);
    setDirty(false);
}

CMouseEventResult VoiceArrow::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (!buttons.isLeftButton())
        return kMouseEventNotHandled;
    pressed = true;
    const int n = std::clamp(Reducer::voicesOf(getValueNormalized()) + direction, 1, Reducer::kVoices);
    beginEdit();
    setValueNormalized(static_cast<float>(Reducer::voicesNorm(n)));
    valueChanged();
    endEdit();
    invalid();
    return kMouseEventHandled;
}

CMouseEventResult VoiceArrow::onMouseUp(CPoint& where, const CButtonState& buttons)
{
    pressed = false;
    invalid();
    return kMouseEventHandled;
}

} // namespace Yonie
