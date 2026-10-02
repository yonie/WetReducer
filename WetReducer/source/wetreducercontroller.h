//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace Yonie {

// Prototype panel: VSTGUI-drawn knobs, no rendered art yet.
class WetReducerController : public Steinberg::Vst::EditControllerEx1, public VSTGUI::VST3EditorDelegate
{
public:
    static Steinberg::FUnknown* createInstance(void* /*context*/)
    {
        return (Steinberg::Vst::IEditController*)new WetReducerController;
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
    Steinberg::tresult performEdit(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;

    VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr name, const VSTGUI::UIAttributes& attributes,
                                    const VSTGUI::IUIDescription* description, VSTGUI::VST3Editor* editor) SMTG_OVERRIDE;

private:
    bool pushing = false;
};

} // namespace Yonie
