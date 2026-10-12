cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

set(vst3sdk_SOURCE_DIR "../vst3sdk")

project(WetReducer
    VERSION 0.1.0.0
    DESCRIPTION "WetReducer VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(WetReducer
    source/version.h
    source/wetreducercids.h
    source/reducerengine.h
    source/reducerengine.cpp
    source/wetreducerprocessor.h
    source/wetreducerprocessor.cpp
    source/wetreducercontroller.h
    source/wetreducercontroller.cpp
    source/wetreducerentry.cpp
    source/weteditor.h
    source/xoverstrip.h
    source/xoverstrip.cpp
    source/steppedknob.h
    source/steppedknob.cpp
    source/bandled.h
    source/bandled.cpp
    source/voicearrow.h
    source/voicearrow.cpp
    source/customviewcreator.h
    source/customviewcreator.cpp
    source/ledmeterview.h
    source/ledmeterview.cpp
    source/numparse.h
    source/meterlevel.h
)

if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(WetReducer PRIVATE resource/wetreducereditor.uidesc)
    target_link_libraries(WetReducer PRIVATE vstgui_support)
    # the rendered panel: backplate, filmstrips and cap sprites (tools/make-assets-wetreducer.py)
    file(GLOB WETREDUCER_PNGS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS resource/*.png)
    smtg_target_add_plugin_resources(WetReducer RESOURCES "resource/wetreducereditor.uidesc" ${WETREDUCER_PNGS})
endif(SMTG_ENABLE_VSTGUI_SUPPORT)

target_compile_features(WetReducer PRIVATE cxx_std_17)
target_link_libraries(WetReducer PRIVATE sdk)
smtg_target_configure_version_file(WetReducer)

if(SMTG_MAC)
    smtg_target_set_bundle(WetReducer
        BUNDLE_IDENTIFIER org.yonie.vst3.wetreducer
        COMPANY_NAME "WET VST"
    )
elseif(SMTG_WIN)
    target_sources(WetReducer PRIVATE resource/win32resource.rc)
endif(SMTG_MAC)


# Audio Unit v2 wrapper for Logic. Only configured with the Xcode generator
# and SMTG_AUDIOUNIT_SDK_PATH pointing at Apple's AudioUnitSDK.
if(SMTG_MAC AND XCODE AND SMTG_ENABLE_AUV2_BUILDS)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${vst3sdk_SOURCE_DIR}/cmake/modules")
    include(SMTG_AddVST3AuV2)
    # The AU version follows the project version; a host caches an AU by it.
    set(AU_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    math(EXPR AU_VERSION_INT "(${PROJECT_VERSION_MAJOR} << 16) | (${PROJECT_VERSION_MINOR} << 8) | ${PROJECT_VERSION_PATCH}")
    configure_file(resource/au-info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/au-info.plist @ONLY)
    smtg_target_add_auv2(WetReducer-au
        BUNDLE_NAME WetReducer
        BUNDLE_IDENTIFIER org.yonie.audiounit.wetreducer
        INFO_PLIST_TEMPLATE ${CMAKE_CURRENT_BINARY_DIR}/au-info.plist
        VST3_PLUGIN_TARGET WetReducer)
endif(SMTG_MAC AND XCODE AND SMTG_ENABLE_AUV2_BUILDS)
