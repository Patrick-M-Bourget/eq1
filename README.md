# eq1

Real-time EQ plugin (VST3, AU, AAX, CLAP and Standalone) for macOS Universal and Windows x64, built on JUCE 9. Vocabulary is in `GLOSSARY.md`; design decisions are in `docs/adr/`.

## Layout

- `engine/` is the DSP Engine: plain C++ with no dependency on JUCE.
- `plugin/` is the Plugin Shell: the JUCE layer that maps host parameters to Engine settings.
- `tests/engine/` tests the Engine through its public interface; `tests/plugin/` drives the Plugin Shell like a host.

## Build and test

Dependencies (JUCE, clap-juce-extensions, Catch2) are fetched by CMake.

```sh
# macOS Universal
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
# Windows x64
cmake -S . -B build -A x64

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Plugins land in `build/plugin/eq1_artefacts/Release/`. CI (`.github/workflows/ci.yml`) also runs pluginval on VST3 and AU and clap-validator on CLAP. AAX is built unsigned and can only be loaded in Pro Tools Developer.
