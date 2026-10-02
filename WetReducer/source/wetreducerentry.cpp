//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------

#include "wetreducerprocessor.h"
#include "wetreducercontroller.h"
#include "wetreducercids.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "WetReducer"

using namespace Steinberg::Vst;
using namespace Yonie;

BEGIN_FACTORY_DEF ("Yonie",
                   "https://wetvst.com",
                   "mailto:contact@wetvst.com")

	DEF_CLASS2 (INLINE_UID_FROM_FUID(kWetReducerProcessorUID),
				PClassInfo::kManyInstances,
				kVstAudioEffectClass,
				stringPluginName,
				Vst::kDistributable,
				WetReducerVST3Category,
				FULL_VERSION_STR,
				kVstVersionString,
				WetReducerProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kWetReducerControllerUID),
				PClassInfo::kManyInstances,
				kVstComponentControllerClass,
				stringPluginName "Controller",
				0,
				"",
				FULL_VERSION_STR,
				kVstVersionString,
				WetReducerController::createInstance)

END_FACTORY
