//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "bandled.h"

#include "vstgui/lib/cdrawcontext.h"
#include "stripframes.h"

#include <algorithm>
#include <cmath>

using namespace VSTGUI;

namespace Yonie {

BandLed::BandLed(const CRect& size, CBitmap* f) : CControl(size, nullptr, -1), frames(f)
{
    setMouseEnabled(false);
    setTransparency(true);
}

void BandLed::draw(CDrawContext* context)
{
    if (!frames)
        return;
    const CRect r = getViewSize();
    // brightness on a curve, so an LED only just lit is still visibly on
    const double v = std::pow(std::clamp(static_cast<double>(getValueNormalized()), 0.0, 1.0), 0.6);
    Strip::draw(context, frames, r, 0);
    if (v > 0.004)
        Strip::draw(context, frames, r, 1, static_cast<float>(v));
    setDirty(false);
}

} // namespace Yonie
