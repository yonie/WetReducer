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
)

target_compile_features(WetReducer PRIVATE cxx_std_17)
target_link_libraries(WetReducer PRIVATE sdk)
smtg_target_configure_version_file(WetReducer)

if(SMTG_MAC)
    smtg_target_set_bundle(WetReducer
        BUNDLE_IDENTIFIER org.yonie.vst3.wetreducer
        COMPANY_NAME "Yonie"
    )
elseif(SMTG_WIN)
    target_sources(WetReducer PRIVATE resource/win32resource.rc)
endif(SMTG_MAC)
