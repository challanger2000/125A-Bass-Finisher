add_executable(BassFinisherDSPContractTests
    bass_dsp_contract_tests.cpp
    ../src/dsp/MetalFinisherDSP.cpp
    ../src/dsp/AdaptiveBandController.cpp
    ../src/dsp/AdaptiveResonanceSuppressor.cpp
    ../src/dsp/AutoLevelCompensator.cpp
    ../src/dsp/ToneMatchDSP.cpp
)

target_include_directories(BassFinisherDSPContractTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp
)

target_compile_features(BassFinisherDSPContractTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherDSPContractTests)

add_test(NAME BassFinisherDSPContractTests COMMAND BassFinisherDSPContractTests)

add_executable(BassFinisherToneMatchTests
    tone_match_dsp_tests.cpp
    ../src/dsp/ToneMatchDSP.cpp
)

target_include_directories(BassFinisherToneMatchTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp
)

target_compile_features(BassFinisherToneMatchTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherToneMatchTests)

add_test(NAME BassFinisherToneMatchTests COMMAND BassFinisherToneMatchTests)


add_executable(BassFinisherAutomationMathTests
    automation_math_tests.cpp
)
target_include_directories(BassFinisherAutomationMathTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_compile_features(BassFinisherAutomationMathTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherAutomationMathTests)
add_test(NAME BassFinisherAutomationMathTests COMMAND BassFinisherAutomationMathTests)

add_executable(BassFinisherBypassCrossfadeTests
    bypass_crossfade_tests.cpp
)
target_include_directories(BassFinisherBypassCrossfadeTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_compile_features(BassFinisherBypassCrossfadeTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherBypassCrossfadeTests)
add_test(NAME BassFinisherBypassCrossfadeTests COMMAND BassFinisherBypassCrossfadeTests)

add_executable(BassFinisherStateV1Tests
    state_v1_tests.cpp
    ../src/HighGainGuitarFinisherProcessor.cpp
    ../src/dsp/MetalFinisherDSP.cpp
    ../src/dsp/AdaptiveBandController.cpp
    ../src/dsp/AdaptiveResonanceSuppressor.cpp
    ../src/dsp/AutoLevelCompensator.cpp
    ../src/dsp/ToneMatchDSP.cpp
    ../src/dsp/ToneMatchAnalyzer.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp
)
target_include_directories(BassFinisherStateV1Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp
)
target_link_libraries(BassFinisherStateV1Tests PRIVATE sdk Threads::Threads)
target_compile_features(BassFinisherStateV1Tests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherStateV1Tests)
add_test(NAME BassFinisherStateV1Tests COMMAND BassFinisherStateV1Tests)


add_executable(BassFinisherProcessorContractTests
    processor_contract_tests.cpp
    ../src/HighGainGuitarFinisherProcessor.cpp
    ../src/dsp/MetalFinisherDSP.cpp
    ../src/dsp/AdaptiveBandController.cpp
    ../src/dsp/AdaptiveResonanceSuppressor.cpp
    ../src/dsp/AutoLevelCompensator.cpp
    ../src/dsp/ToneMatchDSP.cpp
    ../src/dsp/ToneMatchAnalyzer.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/parameterchanges.cpp
)
target_include_directories(BassFinisherProcessorContractTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp
)
target_link_libraries(BassFinisherProcessorContractTests PRIVATE sdk Threads::Threads)
target_compile_features(BassFinisherProcessorContractTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherProcessorContractTests)
add_test(NAME BassFinisherProcessorContractTests COMMAND BassFinisherProcessorContractTests)

add_executable(BassFinisherRealtimeAllocationTests
    realtime_allocation_tests.cpp
    ../src/dsp/MetalFinisherDSP.cpp
    ../src/dsp/AdaptiveBandController.cpp
    ../src/dsp/AdaptiveResonanceSuppressor.cpp
    ../src/dsp/AutoLevelCompensator.cpp
    ../src/dsp/ToneMatchDSP.cpp
)
target_include_directories(BassFinisherRealtimeAllocationTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp
)
target_compile_features(BassFinisherRealtimeAllocationTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherRealtimeAllocationTests)
add_test(NAME BassFinisherRealtimeAllocationTests COMMAND BassFinisherRealtimeAllocationTests)


add_executable(BassFinisherToneMatchQualityTests
    tone_match_quality_tests.cpp
    ../src/dsp/ToneMatchAnalyzer.cpp
)
target_include_directories(BassFinisherToneMatchQualityTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp
)
target_compile_features(BassFinisherToneMatchQualityTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherToneMatchQualityTests)
add_test(NAME BassFinisherToneMatchQualityTests COMMAND BassFinisherToneMatchQualityTests)


add_executable(BassFinisherControllerContractTests
    controller_contract_tests.cpp
    ../src/HighGainGuitarFinisherController.cpp
    ../src/ToneMatchProfileCodec.cpp
    ../src/AudioFileDecoder.cpp
    ../src/ToneMatchReferenceProfileCodec.cpp
    ../src/ToneMatchReferenceService.cpp
    ../src/SteelKnob.cpp
    ../src/SteelPanelView.cpp
    ../src/BrandLogoView.cpp
    ../src/dsp/ToneMatchAnalyzer.cpp
    ../src/dsp/ToneMatchDSP.cpp
    ${vst3sdk_SOURCE_DIR}/public.sdk/source/common/memorystream.cpp
)
target_include_directories(BassFinisherControllerContractTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp
)
target_link_libraries(BassFinisherControllerContractTests PRIVATE
    sdk
    vstgui_support
    bassfinisher_dr_libs
    Threads::Threads
)
target_compile_features(BassFinisherControllerContractTests PRIVATE cxx_std_17)
hggf_enable_project_warnings(BassFinisherControllerContractTests)
add_test(NAME BassFinisherControllerContractTests COMMAND BassFinisherControllerContractTests)


if(MSVC)
    target_link_options(BassFinisherStateV1Tests PRIVATE /STACK:8388608)
    target_link_options(BassFinisherProcessorContractTests PRIVATE /STACK:8388608)
    target_link_options(BassFinisherToneMatchQualityTests PRIVATE /STACK:8388608)
endif()
