add_library(bassfinisher_dr_libs STATIC
    third_party/dr_libs/dr_libs_impl.cpp
)

target_include_directories(bassfinisher_dr_libs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/dr_libs
)

if(MSVC)
    target_compile_options(bassfinisher_dr_libs PRIVATE /W0)
else()
    target_compile_options(bassfinisher_dr_libs PRIVATE -w)
endif()

set(BASS_FINISHER_SOURCES
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
    dsp/AdaptiveBandController.h
    dsp/AdaptiveBandController.cpp
    dsp/AdaptiveResonanceSuppressor.h
    dsp/AdaptiveResonanceSuppressor.cpp
    dsp/AutoLevelCompensator.h
    dsp/AutoLevelCompensator.cpp
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
    BassFinisher
    PACKAGE_NAME "125A-Bass-Finisher-V1"
    SOURCES_LIST ${BASS_FINISHER_SOURCES}
)

target_link_libraries(BassFinisher
    PRIVATE
        sdk
        vstgui_support
        bassfinisher_dr_libs
        Threads::Threads
)

target_compile_features(BassFinisher PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisher)

if(WIN32)
    target_sources(BassFinisher PRIVATE
        ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp
    )
endif()

smtg_target_add_plugin_resources(
    BassFinisher
    RESOURCES
        ../resource/HighGainGuitarFinisher.uidesc
        ../resource/125A_Logo_Master_FINAL.svg
        ../resource/125A_FinisherRing_S_64px_100pct.png
        ../resource/125A_FinisherRing_S_96px_150pct.png
        ../resource/125A_FinisherRing_M_96px_100pct.png
        ../resource/125A_FinisherRing_M_144px_150pct.png
        ../resource/125A_FinisherRing_H_128px_100pct.png
        ../resource/125A_FinisherRing_H_192px_150pct.png
)

smtg_target_configure_version_file(BassFinisher)
