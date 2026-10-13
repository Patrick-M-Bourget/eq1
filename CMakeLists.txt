cmake_minimum_required(VERSION 3.28)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(eq1 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# compile_commands.json in the build folder, which .clangd points editors' language servers at.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Sources and their string literals are UTF-8 on every compiler. Without /utf-8, MSVC writes a narrow
# literal's "–" (typed or as "\u2013") in the machine's code page, cp1252 bytes, so a text test fails on
# Windows only.
if(MSVC)
    add_compile_options(/utf-8)
endif()
# MSVC refuses an initializer list that mixes designated and positional members (C7556), where clang only
# warns, so a Mac build passed and Windows CI failed to build (#199's CPU scenes). Clang makes it an error
# too. eq1's own directories add these options after fetching their dependencies, whose flags stay as they
# are, so their shared build folders (.deps) aren't rebuilt.
set(eq1_compile_options "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>:-Werror=c99-designator>")

option(EQ1_BUILD_PLUGIN "Build the Plugin Shell (needs JUCE)" ON)
option(EQ1_BUILD_TESTS "Build the tests" ON)
# Link-time optimisation is for what ships. scripts/check.sh turns it off: on MSVC it makes each
# format's link several minutes long.
option(EQ1_LTO "Build the Plugin Shell with link-time optimisation" ON)
# So is stripping: shipping binaries keep their debug information beside the bundle, not in it
# (plugin/CMakeLists.txt). Left empty, every configuration but Debug strips, so a Debug build keeps its
# symbols; ON or OFF applies to every configuration. scripts/check.sh sets OFF, so its Release test
# builds keep their symbols too.
set(EQ1_STRIP "" CACHE STRING
    "Strip the Plugin Shell's binaries, keeping a .dSYM or PDB beside each bundle: ON, OFF, or empty for every configuration but Debug")

include(FetchContent)

# A .deps folder is shared by every worktree's universal build (scripts/check.sh), dependencies' build
# folders included. A build for other architectures rebuilds those libraries for its own alone, and the
# other worktrees' links then fail for the architecture it left out. Such a build fetches into a folder
# of its own (as check.sh's sanitizer builds do, under .deps/<name>).
if(APPLE AND DEFINED FETCHCONTENT_BASE_DIR)
    get_filename_component(eq1_deps_name "${FETCHCONTENT_BASE_DIR}" NAME)
    if(eq1_deps_name STREQUAL ".deps" AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "arm64;x86_64")
        message(FATAL_ERROR "FETCHCONTENT_BASE_DIR ${FETCHCONTENT_BASE_DIR} is shared by universal builds, "
            "and this one builds for '${CMAKE_OSX_ARCHITECTURES}'. Configure with "
            "-DCMAKE_OSX_ARCHITECTURES=\"arm64;x86_64\" (scripts/check.sh does), or with a folder of its own: "
            "-DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR}/<name> -DFETCHCONTENT_SOURCE_DIR_<DEP>=<its -src folder>.")
    endif()
endif()

add_subdirectory(engine)

if(EQ1_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()

if(EQ1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
