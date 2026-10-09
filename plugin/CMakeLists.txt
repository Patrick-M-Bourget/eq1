# The Plugin Shell: the JUCE layer around the DSP Engine.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(JUCE)

# CLAP comes from clap-juce-extensions until JUCE ships native CLAP. Pinned to a main commit:
# no release supports JUCE 9 yet (0.26.0 does not). The patch asks the host to rescan parameter
# values after a state load (upstream PR free-audio/clap-juce-extensions#189); drop it once merged.
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DPATCH=${PROJECT_SOURCE_DIR}/cmake/patches/clap-juce-extensions-rescan-after-state-load.patch
        -P ${PROJECT_SOURCE_DIR}/cmake/ApplyPatch.cmake
    SYSTEM
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(clap-juce-extensions)

juce_add_plugin(eq1
    PRODUCT_NAME "eq1"
    COMPANY_NAME "eq1"
    BUNDLE_ID "com.pbourget.eq1"
    PLUGIN_MANUFACTURER_CODE Pmbg
    PLUGIN_CODE Eq01
    FORMATS VST3 AU AAX Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_EQ
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(eq1 PRIVATE
    PluginProcessor.cpp
    Parameters.cpp
    BandEditing.cpp
    BandPanel.cpp
    EqDisplay.cpp
    OutputPanel.cpp
    PluginEditor.cpp
    AnalyzerSpectrum.cpp)

target_compile_definitions(eq1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(eq1
    PRIVATE
        eq1::engine
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(EQ1_LTO)
    target_link_libraries(eq1 PUBLIC juce::juce_recommended_lto_flags)
endif()

clap_juce_extensions_plugin(TARGET eq1
    CLAP_ID "com.pbourget.eq1"
    CLAP_FEATURES audio-effect equalizer mono stereo)
