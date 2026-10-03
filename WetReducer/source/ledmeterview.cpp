//------------------------------------------------------------------------
//------------------------------------------------------------------------

#include "ledmeterview.h"
#include "meterlevel.h"

#include <cmath>

using namespace VSTGUI;

namespace Yonie {

//------------------------------------------------------------------------
// LEDMeterView Implementation
//------------------------------------------------------------------------
LEDMeterView::LEDMeterView(const CRect& size)
    : CControl(size, nullptr, -1)
{
    setWantsFocus(false);
}

//------------------------------------------------------------------------
int LEDMeterView::litSegments() const
{
    const float level = getValueNormalized();

    // A dB-encoded reading: 0 is the floor, so nothing is lit.
    auto dbOf = [&](float v) {
        return dbEncoded ? Meter::decodeDb(v) : 20.0 * std::log10(static_cast<double>(v));
    };

    int litCount;
    if (!thresholds.empty() && static_cast<int>(thresholds.size()) == numSegments)
    {
        // One printed number per segment, so the ladder can be read straight
        // off the panel however unevenly the scale is spaced.
        if (level <= 1e-6f)
            return 0;
        const double db = dbOf(level);
        litCount = 0;
        for (int i = 0; i < numSegments; ++i)
            if (db >= thresholds[i])
                litCount = i + 1;
    }
    else if (dbScale)
    {
        // One segment per printed number. Segment i counts from the bottom and
        // stands for dbLo + i*step, so it lights when the signal has reached
        // the value written beside it.
        if (level <= 1e-6f)
            return 0;
        const double db = dbOf(level);
        const double step = (dbHi - dbLo) / (numSegments - 1);
        litCount = static_cast<int>(std::floor((db - dbLo) / step)) + 1;
    }
    else
    {
        litCount = static_cast<int>(level * numSegments + 0.5f);
    }

    if (litCount > numSegments) litCount = numSegments;
    if (litCount < 0) litCount = 0;
    return litCount;
}

//------------------------------------------------------------------------
void LEDMeterView::draw(CDrawContext* context)
{
    const int litCount = litSegments();
    
    // Black the whole window out first. The backplate has all 12 LEDs painted
    // lit, and the drawn segments do not line up with the painted ones, so
    // anything left uncovered shows through as a bright sliver.
    context->setFillColor(windowBlack);
    context->drawRect(getViewSize(), kDrawFilled);

    // Glow first, so the segments themselves land on top of it and stay crisp.
    // Rings step outwards from each lit segment with the alpha falling off, and
    // they are drawn for every lit LED before any segment is filled - otherwise
    // a neighbour's glow would paint over the LED below it.
    for (int i = 0; i < litCount; i++)
    {
        CRect seg = calculateSegmentRect(i);
        CColor glow = getLitColor(i);
        for (int s = glowSpread; s >= 1; s--)
        {
            CRect halo = seg;
            halo.extend(static_cast<CCoord>(s), static_cast<CCoord>(s));
            glow.alpha = static_cast<uint8_t>(glowAlpha * (glowSpread - s + 1)
                                              / (glowSpread + 1));
            context->setFillColor(glow);
            context->drawRect(halo, kDrawFilled);
        }
    }

    // Draw each segment
    for (int i = 0; i < numSegments; i++)
    {
        CRect segRect = calculateSegmentRect(i);
        CColor color = (i < litCount) ? getLitColor(i) : getDarkColor(i);
        
        context->setFillColor(color);
        context->drawRect(segRect, kDrawFilled);
    }
    
    setDirty(false);
}

//------------------------------------------------------------------------
CRect LEDMeterView::calculateSegmentRect(int segmentIndex) const
{
    CRect viewSize = getViewSize();
    CCoord totalWidth = viewSize.getWidth();
    CCoord totalHeight = viewSize.getHeight();
    
    // inverted flips which end index 0 sits at: a level meter starts at the
    // bottom (or the left) and grows towards the danger end, a gain-reduction
    // meter starts at 0 dB and grows downwards.
    const int i = inverted ? (numSegments - 1 - segmentIndex) : segmentIndex;

    if (isHorizontal)
    {
        // Horizontal meter: segments arranged left to right
        CCoord segmentWidth = (totalWidth - (numSegments - 1) * segmentGap) / numSegments;
        CCoord x = viewSize.left + i * (segmentWidth + segmentGap);
        
        return CRect(x, viewSize.top, x + segmentWidth, viewSize.bottom);
    }
    else
    {
        // Vertical meter: segments arranged bottom to top
        CCoord segmentHeight = (totalHeight - (numSegments - 1) * segmentGap) / numSegments;
        CCoord y = viewSize.bottom - (i + 1) * segmentHeight - i * segmentGap;
        
        return CRect(viewSize.left, y, viewSize.right, y + segmentHeight);
    }
}

//------------------------------------------------------------------------
// Which zone a segment belongs to: 0 green, 1 yellow, 2 red.
int LEDMeterView::zoneOf(int segmentIndex) const
{
    // Measured boundaries win when the panel supplied them.
    if (zoneYellow >= 0 && zoneRed >= 0)
    {
        if (segmentIndex >= zoneRed) return 2;
        if (segmentIndex >= zoneYellow) return 1;
        return 0;
    }

    // Otherwise the old proportional split: 12 segments as 0-7 green,
    // 8-9 yellow, 10-11 red, scaled to whatever count is in use.
    const float position = static_cast<float>(segmentIndex) / (numSegments - 1);
    if (position < 0.67f) return 0;
    if (position < 0.83f) return 1;
    return 2;
}

//------------------------------------------------------------------------
CColor LEDMeterView::getLitColor(int segmentIndex) const
{
    if (reduction)
        return amberLit;

    switch (zoneOf(segmentIndex))
    {
        case 0:  return greenLit;
        case 1:  return yellowLit;
        default: return redLit;
    }
}

//------------------------------------------------------------------------
CColor LEDMeterView::getDarkColor(int segmentIndex) const
{
    if (reduction)
        return amberDark;

    switch (zoneOf(segmentIndex))
    {
        case 0:  return greenDark;
        case 1:  return yellowDark;
        default: return redDark;
    }
}

//------------------------------------------------------------------------
} // namespace Yonie