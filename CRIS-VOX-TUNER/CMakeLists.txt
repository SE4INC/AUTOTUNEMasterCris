cmake_minimum_required(VERSION 3.24)
project(CrisVoxTuner VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(CRIS_BUILD_PLUGIN "Build JUCE VST3 and standalone" ON)
option(CRIS_BUILD_TESTS "Build DSP tests" ON)
include(FetchContent)
FetchContent_Declare(signalsmith-linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG 7bb1c8f36794d6980080ce817c91f8da8fe56a66)
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da
    GIT_SUBMODULES "")
FetchContent_MakeAvailable(signalsmith-linear signalsmith-stretch)

add_library(cris_dsp STATIC
    Source/dsp/PitchDetector.cpp
    Source/dsp/PitchTracker.cpp
    Source/dsp/PitchCorrection.cpp
    Source/dsp/FormantProcessor.cpp
    Source/dsp/VocalEngine.cpp)
target_include_directories(cris_dsp PUBLIC Source)
target_link_libraries(cris_dsp PUBLIC signalsmith-stretch)
if(MSVC)
    target_compile_options(cris_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(cris_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CRIS_BUILD_TESTS)
    enable_testing()
    add_executable(cris_dsp_tests tests/DspTests.cpp)
    target_link_libraries(cris_dsp_tests PRIVATE cris_dsp)
    add_test(NAME cris_dsp_tests COMMAND cris_dsp_tests)
endif()

if(CRIS_BUILD_PLUGIN)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2
        GIT_SUBMODULES "")
    FetchContent_MakeAvailable(JUCE)
    juce_add_plugin(CrisVoxTuner
        COMPANY_NAME "CRIS Audio"
        BUNDLE_ID "audio.cris.voxtuner"
        PLUGIN_MANUFACTURER_CODE CrAu
        PLUGIN_CODE CvTn
        FORMATS VST3 Standalone
        PRODUCT_NAME "CRIS VOX TUNER"
        DESCRIPTION "Monophonic vocal pitch correction and clip editing"
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Pitch\ Shift)
    target_sources(CrisVoxTuner PRIVATE
        Source/PluginProcessor.cpp Source/PluginProcessor.h
        Source/PluginEditor.cpp Source/PluginEditor.h
        Source/model/ClipService.cpp Source/model/ClipService.h
        Source/model/PresetManager.h
        Source/ui/PitchEditor.cpp Source/ui/PitchEditor.h)
    target_compile_definitions(CrisVoxTuner PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(CrisVoxTuner PRIVATE cris_dsp
        juce::juce_audio_utils juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags)
    if(CRIS_BUILD_TESTS)
        add_executable(cris_integration_tests tests/IntegrationTests.cpp)
        target_include_directories(cris_integration_tests PRIVATE Source "${juce_SOURCE_DIR}/modules")
        target_compile_definitions(cris_integration_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
            CRIS_TEST_VOCAL="${juce_SOURCE_DIR}/examples/Assets/singing.ogg")
        target_link_libraries(cris_integration_tests PRIVATE CrisVoxTuner cris_dsp)
        add_test(NAME cris_integration_tests COMMAND cris_integration_tests)
    endif()
endif()
