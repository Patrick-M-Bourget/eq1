# The DSP Engine: plain C++, no dependency on JUCE's plugin wrapper or UI.
add_library(eq1_engine STATIC
    src/Engine.cpp
    src/Band.cpp
    src/MatchedDesign.cpp
    src/ShapeDesign.cpp
)
add_library(eq1::engine ALIAS eq1_engine)

target_include_directories(eq1_engine PUBLIC include PRIVATE src)
target_compile_features(eq1_engine PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(eq1_engine PRIVATE /W4)
else()
    target_compile_options(eq1_engine PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()
