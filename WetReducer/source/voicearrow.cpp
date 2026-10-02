//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "voicearrow.h"
#include "reducerengine.h"

#include "vstgui/lib/cdrawcontext.h"

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
        frames->draw(context, getViewSize(), CPoint(0, pressed ? frames->getHeight() / 2.0 : 0.0));
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
