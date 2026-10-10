FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

include(Catch)

# One source per line, sorted, so tickets adding tests in parallel merge cleanly.
add_executable(eq1_engine_tests
    engine/AllocationGuard.cpp
    engine/AnalysisTapTest.cpp
    engine/BandSlotTest.cpp
    engine/BellResponseTest.cpp
    engine/BellStabilityTest.cpp
    engine/ClickFreeTest.cpp
    engine/DynamicsTest.cpp
    engine/OutputLevelTest.cpp
    engine/OutputTest.cpp
    engine/RealtimeSafetyTest.cpp
    engine/ResponseTest.cpp
    engine/RobustnessTest.cpp
    engine/SampleRateAndBlockSizeTest.cpp
    engine/SettingsHandoffTest.cpp
    engine/ShapeResponseTest.cpp
    engine/SoloTest.cpp
    engine/StereoPlacementTest.cpp
)
target_link_libraries(eq1_engine_tests PRIVATE eq1::engine Catch2::Catch2WithMain)
catch_discover_tests(eq1_engine_tests ADD_TAGS_AS_LABELS)

if(EQ1_BUILD_PLUGIN)
    # Host-level checks drive the Plugin Shell the way a host does. One source per line, as above.
    add_executable(eq1_plugin_tests
        plugin/ABCompareTest.cpp
        plugin/AnalyzerSpectrumTest.cpp
        plugin/BandClipboardTest.cpp
        plugin/BandEditingTest.cpp
        plugin/BandMenuTest.cpp
        plugin/BandPanelTest.cpp
        plugin/DisplayRangeTest.cpp
        plugin/EditHistoryTest.cpp
        plugin/EqDisplayTest.cpp
        plugin/HostSessionTest.cpp
        plugin/OutputMeterTest.cpp
        plugin/PluginShellTest.cpp
        plugin/PresetLibraryTest.cpp
        plugin/PresetTest.cpp
        plugin/StapleTest.cpp
        plugin/StateTest.cpp
    )
    target_include_directories(eq1_plugin_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/plugin
        $<TARGET_PROPERTY:eq1,INCLUDE_DIRECTORIES>)
    target_compile_definitions(eq1_plugin_tests PRIVATE
        $<TARGET_PROPERTY:eq1,COMPILE_DEFINITIONS>
        EQ1_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/plugin/fixtures")
    target_link_libraries(eq1_plugin_tests PRIVATE eq1 eq1::engine Catch2::Catch2WithMain)
    catch_discover_tests(eq1_plugin_tests)
endif()

if(EQ1_BUILD_PLUGIN)
    # Hosts the built plugin through its VST3 or AU wrapper (scripts/check.sh validate).
    juce_add_console_app(eq1_sidechain_routing_check PRODUCT_NAME "eq1_sidechain_routing_check")
    target_sources(eq1_sidechain_routing_check PRIVATE host/SidechainRoutingCheck.cpp)
    target_compile_definitions(eq1_sidechain_routing_check PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(eq1_sidechain_routing_check PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
    if(APPLE)
        target_link_libraries(eq1_sidechain_routing_check PRIVATE "-framework CoreAudioKit")
    endif()
endif()

# Measures the Engine's CPU load against its budget (scripts/check.sh cpu). Not a ctest test: it
# needs the machine to itself.
add_executable(eq1_cpu_budget cpu/CpuBudget.cpp)
target_link_libraries(eq1_cpu_budget PRIVATE eq1::engine)
