FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

include(Catch)

add_executable(eq1_engine_tests
    engine/AllocationGuard.cpp
    engine/AnalysisTapTest.cpp
    engine/BandSlotTest.cpp
    engine/BellResponseTest.cpp
    engine/BellStabilityTest.cpp
    engine/ClickFreeTest.cpp
    engine/RealtimeSafetyTest.cpp
    engine/SettingsHandoffTest.cpp
    engine/StereoPlacementTest.cpp
    engine/ShapeResponseTest.cpp
)
target_link_libraries(eq1_engine_tests PRIVATE eq1::engine Catch2::Catch2WithMain)
catch_discover_tests(eq1_engine_tests ADD_TAGS_AS_LABELS)

if(EQ1_BUILD_PLUGIN)
    # Host-level checks drive the Plugin Shell the way a host does.
    add_executable(eq1_plugin_tests plugin/PluginShellTest.cpp)
    target_include_directories(eq1_plugin_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/plugin
        $<TARGET_PROPERTY:eq1,INCLUDE_DIRECTORIES>)
    target_compile_definitions(eq1_plugin_tests PRIVATE
        $<TARGET_PROPERTY:eq1,COMPILE_DEFINITIONS>)
    target_link_libraries(eq1_plugin_tests PRIVATE eq1 eq1::engine Catch2::Catch2WithMain)
    catch_discover_tests(eq1_plugin_tests)
endif()
