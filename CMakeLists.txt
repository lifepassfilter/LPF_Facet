cmake_minimum_required(VERSION 3.22)
project(LPF_Facet VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# JUCE: either point JUCE_DIR at a local checkout, or let CMake download it.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FACET_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Spatial.cpp)

# Formats: add AAX here if you have the AAX SDK (juce_set_aax_sdk_path).
set(FACET_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FACET_FORMATS AU)
endif()

juce_add_binary_data(FacetAssets SOURCES
    fonts/AtkinsonHyperlegible-Regular.ttf
    fonts/AtkinsonHyperlegible-Bold.ttf
    fonts/OpenDyslexic-Regular.otf
    fonts/OpenDyslexic-Bold.otf
    fonts/Rajdhani-Bold.ttf)

function(facet_target target product code bundle)
    juce_add_plugin(${target}
        COMPANY_NAME "Life Pass Filter"
        COMPANY_WEBSITE "https://lifepassfilter.com"
        BUNDLE_ID "com.lifepassfilter.${bundle}"
        PLUGIN_MANUFACTURER_CODE Lpfx
        PLUGIN_CODE ${code}
        PRODUCT_NAME "${product}"
        FORMATS ${FACET_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES ${ARGN})

    target_sources(${target} PRIVATE ${FACET_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${target}
        PRIVATE FacetAssets juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags
               juce::juce_recommended_lto_flags
               juce::juce_recommended_warning_flags)
endfunction()

# One plugin: Audio, MIDI or Both modes (an effect that also takes MIDI notes)
facet_target(LPF_Facet "Facet" Fct1 facet Fx)

# Offline render test (no GUI): renders a WAV through the engine.
option(FACET_BUILD_TESTS "Build offline render test" OFF)
if(FACET_BUILD_TESTS)
    juce_add_console_app(FacetRenderTest PRODUCT_NAME "FacetRenderTest")
    target_sources(FacetRenderTest PRIVATE Tests/RenderTest.cpp Source/Spatial.cpp)
    target_include_directories(FacetRenderTest PRIVATE Source)
    target_compile_definitions(FacetRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FacetRenderTest PRIVATE juce::juce_audio_formats juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    enable_testing()
    add_test(NAME render_and_determinism COMMAND FacetRenderTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
