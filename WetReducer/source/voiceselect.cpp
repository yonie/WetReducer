//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "voiceselect.h"
#include "reducerengine.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"

#include <cmath>

using namespace VSTGUI;

namespace Yonie {

namespace {
const CColor kGlass(0xa9, 0xb8, 0x6a);      // the LCD's yellow-green glass
const CColor kGhost(0x98, 0xa6, 0x5e);      // unlit segments still show faintly
const CColor kSeg(0x1c, 0x22, 0x10);        // lit segments, near black
const CColor kBezel(0x10, 0x10, 0x12);
const CColor kCap(0xe9, 0xe5, 0xdb);
const CColor kCapDown(0xc9, 0xc5, 0xbb);
const CColor kArrow(0x36, 0x36, 0x34);

// Which of the seven segments (a b c d e f g) each digit lights.
const unsigned char kDigits[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};

void drawDigit(CDrawContext* ctx, const CRect& r, int digit, const CColor& on, const CColor& off)
{
    const double w = r.getWidth(), h = r.getHeight();
    const double t = std::max(2.0, w * 0.17);        // segment thickness
    const double x0 = r.left, x1 = r.right, y0 = r.top, ym = r.top + h / 2, y1 = r.bottom;
    const double g = 1.0;                            // gap between segments
    const CRect seg[7] = {
        CRect(x0 + t * 0.6 + g, y0, x1 - t * 0.6 - g, y0 + t),                      // a top
        CRect(x1 - t, y0 + t * 0.6 + g, x1, ym - t * 0.4 - g),                      // b top right
        CRect(x1 - t, ym + t * 0.4 + g, x1, y1 - t * 0.6 - g),                      // c bottom right
        CRect(x0 + t * 0.6 + g, y1 - t, x1 - t * 0.6 - g, y1),                      // d bottom
        CRect(x0, ym + t * 0.4 + g, x0 + t, y1 - t * 0.6 - g),                      // e bottom left
        CRect(x0, y0 + t * 0.6 + g, x0 + t, ym - t * 0.4 - g),                      // f top left
        CRect(x0 + t * 0.6 + g, ym - t / 2, x1 - t * 0.6 - g, ym + t / 2)};        // g middle
    const unsigned char bits = kDigits[digit % 10];
    for (int i = 0; i < 7; ++i)
    {
        ctx->setFillColor((bits >> i) & 1 ? on : off);
        ctx->drawRect(seg[i], kDrawFilled);
    }
}

void drawArrow(CDrawContext* ctx, const CRect& r, bool up)
{
    auto path = owned(ctx->createGraphicsPath());
    if (!path)
        return;
    const CPoint c = r.getCenter();
    const double s = std::min(r.getWidth(), r.getHeight()) * 0.22;
    if (up)
    {
        path->beginSubpath(CPoint(c.x, c.y - s));
        path->addLine(CPoint(c.x + s, c.y + s * 0.7));
        path->addLine(CPoint(c.x - s, c.y + s * 0.7));
    }
    else
    {
        path->beginSubpath(CPoint(c.x, c.y + s));
        path->addLine(CPoint(c.x + s, c.y - s * 0.7));
        path->addLine(CPoint(c.x - s, c.y - s * 0.7));
    }
    path->closeSubpath();
    ctx->setFillColor(kArrow);
    ctx->drawGraphicsPath(path, CDrawContext::kPathFilled);
}
} // namespace

VoiceSelect::VoiceSelect(const CRect& size) : CControl(size, nullptr, -1)
{
    setTransparency(true);
}

CRect VoiceSelect::lcd() const
{
    const CRect r = getViewSize();
    return CRect(r.left, r.top, r.left + r.getHeight() * 1.1, r.bottom);
}

CRect VoiceSelect::upButton() const
{
    const CRect r = getViewSize(), l = lcd();
    const double x = l.right + 10, h = (r.getHeight() - 4) / 2;
    return CRect(x, r.top, x + h * 1.5, r.top + h);
}

CRect VoiceSelect::downButton() const
{
    const CRect u = upButton();
    const double h = u.getHeight();
    return CRect(u.left, u.bottom + 4, u.right, u.bottom + 4 + h);
}

void VoiceSelect::draw(CDrawContext* ctx)
{
    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);
    const int n = Reducer::voicesOf(getValueNormalized());

    // the LCD: a dark bezel, the glass, one seven-segment digit with its ghost
    const CRect l = lcd();
    ctx->setFillColor(kBezel);
    ctx->drawRect(l, kDrawFilled);
    CRect glass = l;
    glass.inset(3, 3);
    ctx->setFillColor(kGlass);
    ctx->drawRect(glass, kDrawFilled);
    CRect digit = glass;
    digit.inset(glass.getWidth() * 0.26, glass.getHeight() * 0.14);
    drawDigit(ctx, digit, 8, kGhost, kGhost);
    drawDigit(ctx, digit, n, kSeg, CColor(0, 0, 0, 0));

    // the two tip buttons
    for (int dir : {1, -1})
    {
        const CRect b = dir > 0 ? upButton() : downButton();
        ctx->setFillColor(CColor(0, 0, 0, 90));
        ctx->drawRect(CRect(b.left + 1, b.top + 2, b.right + 1, b.bottom + 2), kDrawFilled);
        ctx->setFillColor(pressed == dir ? kCapDown : kCap);
        ctx->drawRect(b, kDrawFilled);
        drawArrow(ctx, b, dir > 0);
    }
    setDirty(false);
}

CMouseEventResult VoiceSelect::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    if (!buttons.isLeftButton())
        return kMouseEventNotHandled;
    const int dir = upButton().pointInside(where) ? 1 : downButton().pointInside(where) ? -1 : 0;
    if (dir == 0)
        return kMouseEventHandled;
    pressed = dir;
    const int n = std::clamp(Reducer::voicesOf(getValueNormalized()) + dir, 1, Reducer::kVoices);
    beginEdit();
    setValueNormalized(static_cast<float>(Reducer::voicesNorm(n)));
    valueChanged();
    endEdit();
    invalid();
    return kMouseEventHandled;
}

CMouseEventResult VoiceSelect::onMouseUp(CPoint& where, const CButtonState& buttons)
{
    pressed = 0;
    invalid();
    return kMouseEventHandled;
}

} // namespace Yonie
