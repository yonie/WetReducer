//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// stripviews.h - VSTGUI's on/off button and movie bitmap, drawing their frames
// from a filmstrip with gutters (see stripframes.h). Created by name from the
// .uidesc: class="COnOffButton" custom-view-name="StripButton", and
// class="CMovieBitmap" custom-view-name="StripMovie".
//------------------------------------------------------------------------

#pragma once

#include "stripframes.h"

#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/cmoviebitmap.h"

namespace Yonie {

class StripButton : public VSTGUI::COnOffButton
{
public:
    explicit StripButton(const VSTGUI::CRect& size) : COnOffButton(size) {}

    void draw(VSTGUI::CDrawContext* context) override
    {
        Strip::draw(context, getDrawBackground(), getViewSize(), getValue() == getMax() ? 1 : 0);
        setDirty(false);
    }
};

class StripMovie : public VSTGUI::CMovieBitmap
{
public:
    explicit StripMovie(const VSTGUI::CRect& size) : CMovieBitmap(size, nullptr, -1, nullptr) {}

    void draw(VSTGUI::CDrawContext* context) override
    {
        auto* bitmap = getDrawBackground();
        const int count = Strip::frameCount(bitmap, getViewSize().getHeight());
        Strip::draw(context, bitmap, getViewSize(), Strip::frameFor(getValueNormalized(), count));
        setDirty(false);
    }
};

} // namespace Yonie
