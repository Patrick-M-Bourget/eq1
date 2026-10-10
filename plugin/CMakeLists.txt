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
    BandMenu.cpp
    BandPanel.cpp
    DetectionRangeBar.cpp
    DynamicRangeRing.cpp
    DynamicsSection.cpp
    EqDisplay.cpp
    display/AnalyzerLayer.cpp
    display/CurvesLayer.cpp
    display/DisplayGeometry.cpp
    display/EdgeFadeLayer.cpp
    display/GridLayer.cpp
    display/HandlesLayer.cpp
    DisplayRange.cpp
    DisplayRangeChip.cpp
    EditHistory.cpp
    ABCompare.cpp
    PresetSettings.cpp
    BandClipboard.cpp
    PresetLibrary.cpp
    PresetBar.cpp
    PresetBrowser.cpp
    OutputPopover.cpp
    HeaderBar.cpp
    AnalyzerPopover.cpp
    FooterBar.cpp
    OutputMeter.cpp
    PluginEditor.cpp
    AnalyzerSpectrum.cpp
    staple/Fonts.cpp
    staple/Icons.cpp
    staple/LookAndFeel.cpp
    Accessibility.cpp
    staple/controls/EdgeSelector.cpp
    staple/controls/IconButton.cpp
    staple/controls/Knob.cpp
    staple/controls/KnobTooltip.cpp
    staple/controls/Overlay.cpp
    staple/controls/ParseValue.cpp
    staple/controls/Popover.cpp
    staple/controls/TextChip.cpp
    KeyboardSlider.cpp
    KeyboardControl.cpp
    UserSettings.cpp)

# The Factory Presets, bundled into the plugin.
juce_add_binary_data(eq1_factory_presets
    NAMESPACE FactoryPresets
    HEADER_NAME FactoryPresets.h
    SOURCES
        ${PROJECT_SOURCE_DIR}/presets/factory/Default.eq1preset
        "${PROJECT_SOURCE_DIR}/presets/factory/De-Esser.eq1preset"
        "${PROJECT_SOURCE_DIR}/presets/factory/Kick Punch.eq1preset"
        "${PROJECT_SOURCE_DIR}/presets/factory/Master Polish.eq1preset"
        "${PROJECT_SOURCE_DIR}/presets/factory/Vocal Presence.eq1preset")

# Staple's one font, Manrope (SIL Open Font License 1.1, assets/fonts/OFL.txt), bundled so the editor
# never falls back to a system font.
juce_add_binary_data(eq1_staple_fonts
    NAMESPACE StapleFonts
    HEADER_NAME StapleFonts.h
    SOURCES
        ${PROJECT_SOURCE_DIR}/assets/fonts/manrope-regular.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/manrope-medium.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/manrope-semibold.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/manrope-bold.ttf)

target_compile_definitions(eq1 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(eq1
    PRIVATE
        eq1::engine
        eq1_factory_presets
        eq1_staple_fonts
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
