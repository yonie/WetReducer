//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "bandled.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"

#include <algorithm>
#include <cmath>

using namespace VSTGUI;

namespace Yonie {

namespace {
CColor mix(const CColor& a, const CColor& b, double t, uint8_t alpha = 255)
{
    auto ch = [t](uint8_t x, uint8_t y) { return static_cast<uint8_t>(std::lround(x + (y - x) * t)); };
    return CColor(ch(a.red, b.red), ch(a.green, b.green), ch(a.blue, b.blue), alpha);
}
} // namespace

BandLed::BandLed(const CRect& size, const CColor& colour) : CControl(size, nullptr, -1), colour(colour)
{
    setMouseEnabled(false);
    setTransparency(true);
}

void BandLed::draw(CDrawContext* context)
{
    // brightness on a curve, so a lamp that is only just catching is still visibly on
    const double v = std::pow(std::clamp(static_cast<double>(getValueNormalized()), 0.0, 1.0), 0.6);
    const CRect r = getViewSize();
    const CPoint c = r.getCenter();
    const double lens = 5.5;                       // radius of the lamp itself
    const double halo = std::min(r.getWidth(), r.getHeight()) / 2.0;
    const CColor dark(0x1d, 0x1e, 0x21);

    context->setDrawMode(kAntiAliasing | kNonIntegralMode);

    // Glow on the plate around the lamp, grows with the light.
    if (v > 0.02)
    {
        for (int i = 8; i >= 1; --i)
        {
            const double rr = lens + (halo - lens) * i / 8.0;
            const double a = v * 150.0 * (1.0 - i / 9.0) / 8.0;
            context->setFillColor(CColor(colour.red, colour.green, colour.blue, static_cast<uint8_t>(a)));
            context->drawEllipse(CRect(c.x - rr, c.y - rr, c.x + rr, c.y + rr), kDrawFilled);
        }
    }

    // Bezel, then the lens: dull band colour when dark, the band colour when lit, and a hot
    // white-ish core when it is fully on.
    context->setFillColor(CColor(0x10, 0x10, 0x12));
    context->drawEllipse(CRect(c.x - lens - 1.5, c.y - lens - 1.5, c.x + lens + 1.5, c.y + lens + 1.5), kDrawFilled);
    context->setFillColor(mix(mix(dark, colour, 0.28), colour, v));
    context->drawEllipse(CRect(c.x - lens, c.y - lens, c.x + lens, c.y + lens), kDrawFilled);
    if (v > 0.5)
    {
        const double core = lens * 0.55;
        context->setFillColor(mix(colour, CColor(255, 250, 235), 0.6, static_cast<uint8_t>((v - 0.5) * 2.0 * 255)));
        context->drawEllipse(CRect(c.x - core, c.y - core, c.x + core, c.y + core), kDrawFilled);
    }
    // a small reflection, top left, so it reads as a lens when off
    context->setFillColor(CColor(255, 255, 255, 40));
    context->drawEllipse(CRect(c.x - lens * 0.6, c.y - lens * 0.7, c.x - lens * 0.05, c.y - lens * 0.15), kDrawFilled);
    setDirty(false);
}

} // namespace Yonie
