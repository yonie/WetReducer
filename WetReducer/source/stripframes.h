//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// stripframes.h - drawing one frame of a filmstrip without its neighbours.
//
// Every control's frames are cut from the panel render and stacked in one PNG.
// Drawn at any zoom but 100%, the bitmap is scaled and the filter reads a row
// past the frame's edge - the next frame's top row, which is a different patch
// of panel (plate where the frame's own bottom is a coloured box, or an icon).
// That showed as thin lines under the band LEDs and dashes under the knobs.
// So tools/make-assets-wetreducer.py leaves kGutter rows above and below every
// frame, filled with that frame's own edge rows, and these views skip them.
// A view's height is one frame's height.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"

#include <algorithm>

namespace Yonie {
namespace Strip {

constexpr int kGutter = 2;   // must match GUTTER in make-assets-wetreducer.py

inline int frameCount(VSTGUI::CBitmap* bitmap, VSTGUI::CCoord frameHeight)
{
    if (!bitmap || frameHeight <= 0)
        return 0;
    return static_cast<int>(bitmap->getHeight() / (frameHeight + 2 * kGutter) + 0.5);
}

// The frame a normalised value shows, the way VSTGUI picks one for a multi-frame bitmap.
inline int frameFor(float normalized, int count)
{
    if (count <= 1)
        return 0;
    const int f = static_cast<int>(std::clamp(normalized, 0.f, 1.f) * count);
    return std::min(f, count - 1);
}

inline void draw(VSTGUI::CDrawContext* context, VSTGUI::CBitmap* bitmap, const VSTGUI::CRect& rect,
                 int frame, float alpha = 1.f)
{
    if (!bitmap)
        return;
    const VSTGUI::CCoord h = rect.getHeight();
    bitmap->draw(context, rect, VSTGUI::CPoint(0, frame * (h + 2 * kGutter) + kGutter), alpha);
}

} // namespace Strip
} // namespace Yonie
