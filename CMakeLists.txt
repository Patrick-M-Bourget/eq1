cmake_minimum_required(VERSION 3.28)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(eq1 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# compile_commands.json in the build folder, which .clangd points editors' language servers at.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Sources and their string literals are UTF-8 on every compiler. Without /utf-8, MSVC reads a literal's
# "–" or "\u2013" in the machine's code page and writes cp1252 bytes, so a text test fails on Windows only.
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(EQ1_BUILD_PLUGIN "Build the Plugin Shell (needs JUCE)" ON)
option(EQ1_BUILD_TESTS "Build the tests" ON)
# Link-time optimisation is for what ships. scripts/check.sh turns it off: on MSVC it makes each
# format's link several minutes long.
option(EQ1_LTO "Build the Plugin Shell with link-time optimisation" ON)

include(FetchContent)

add_subdirectory(engine)

if(EQ1_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()

if(EQ1_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
