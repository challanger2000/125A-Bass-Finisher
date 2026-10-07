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
