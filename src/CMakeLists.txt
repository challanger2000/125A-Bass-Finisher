add_library(hggf_dr_libs STATIC
    third_party/dr_libs/dr_libs_impl.cpp
)

target_include_directories(hggf_dr_libs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dr_libs
)

if(MSVC)
    target_compile_options(hggf_dr_libs PRIVATE /W0)
else()
    target_compile_options(hggf_dr_libs PRIVATE -w)
endif()

set(HIGH_GAIN_GUITAR_FINISHER_SOURCES
    HighGainGuitarFinisherIDs.h
    HighGainGuitarFinisherProcessor.h
    HighGainGuitarFinisherProcessor.cpp
    HighGainGuitarFinisherController.h
    HighGainGuitarFinisherController.cpp
    ToneMatchStateIO.h
    ToneMatchMessage.h
    ToneMatchProfileCodec.h
    ToneMatchProfileCodec.cpp
    AudioFileDecoder.h
    AudioFileDecoder.cpp
    ToneMatchReferenceProfileCodec.h
    ToneMatchReferenceProfileCodec.cpp
    ToneMatchReferenceService.h
    ToneMatchReferenceService.cpp
    ToneMatchStatus.h
    third_party/dr_libs/dr_wav.h
    third_party/dr_libs/dr_flac.h
    third_party/dr_libs/dr_mp3.h
    third_party/dr_libs/LICENSE
    third_party/dr_libs/README_125A.txt
    SteelKnob.h
    SteelKnob.cpp
    SteelPanelView.h
    SteelPanelView.cpp
    BrandLogoView.h
    BrandLogoView.cpp
    dsp/Biquad.h
    dsp/LowCutMapping.h
    dsp/DelayDivisionMapping.h
    dsp/AdaptiveBandController.h
    dsp/AdaptiveBandController.cpp
    dsp/AdaptiveResonanceSuppressor.h
    dsp/AdaptiveResonanceSuppressor.cpp
    dsp/AutoLevelCompensator.h
    dsp/AutoLevelCompensator.cpp
    dsp/IndustrialRoom.h
    dsp/IndustrialRoom.cpp
    dsp/LeadDelay.h
    dsp/LeadDelay.cpp
    dsp/ToneMatchProfile.h
    dsp/ToneMatchProfileMailbox.h
    dsp/ToneMatchCaptureBuffer.h
    dsp/ToneMatchCaptureConsumer.h
    dsp/ToneMatchCaptureWorker.h
    dsp/ToneMatchAnalyzer.h
    dsp/ToneMatchAnalyzer.cpp
    dsp/ToneMatchDSP.h
    dsp/ToneMatchDSP.cpp
    dsp/MetalFinisherDSP.h
    dsp/MetalFinisherDSP.cpp
    entry.cpp
)

smtg_add_vst3plugin(
    HighGainGuitarFinisher
    PACKAGE_NAME "125A-High-Gain-Guitar-Finisher-V3"
    SOURCES_LIST ${HIGH_GAIN_GUITAR_FINISHER_SOURCES}
)

target_link_libraries(HighGainGuitarFinisher
    PRIVATE
        sdk
        vstgui_support
        hggf_dr_libs
        Threads::Threads
)

target_compile_features(HighGainGuitarFinisher PRIVATE cxx_std_17)
hggf_enable_project_warnings(HighGainGuitarFinisher)

if(WIN32)
    target_sources(HighGainGuitarFinisher PRIVATE
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp
    )
endif()

smtg_target_add_plugin_resources(
    HighGainGuitarFinisher
    RESOURCES
        ../resource/HighGainGuitarFinisher.uidesc
        ../resource/125A_Logo_Master_FINAL.svg
)

smtg_target_configure_version_file(HighGainGuitarFinisher)
