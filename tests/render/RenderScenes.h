#pragma once

// The null check's render scenes (docs/performance.md, "Null check"): fixed, seeded scenes that
// scripts/check.sh null renders through two builds' Engines and compares sample for sample. The list
// is data the render tool (RenderTool.cpp) and the comparison share: a scene added here is rendered and
// compared on both sides. Every scene is stereo, renderSeconds long, in renderBlockSize-sample blocks (not
// a multiple of the Engine's 16-sample run), at each of renderSampleRates.

#include "../engine/Measure.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace eq1::test
{

inline constexpr std::array renderSampleRates = { 44100.0, 96000.0, 192000.0 };
inline constexpr int renderBlockSize = 97;
inline constexpr int renderChannels = 2;
inline constexpr double renderSeconds = 1.0;
// When a scene's change of settings arrives: late enough for the first settings to be heard, early
// enough for the glide or crossfade (about 50 ms) and what follows to be rendered.
inline constexpr double renderChangeSeconds = 0.25;

struct RenderScene
{
    std::string name; // lowercase letters, digits and hyphens: the output's file name
    Settings settings;
    std::optional<Settings> change; // settings from renderChangeSeconds on, for glides and crossfades
    bool sidechain = false;         // fed noise bursts on the Sidechain input
};

namespace detail
{
    // One Band of a Shape at 1 kHz. Its Gain is high so that its part of the output is loud: a change of
    // 1e-5 dB to a Band's Gain alone moves the output by about 1e-6 of that part, the bar's -120 dBFS.
    inline constexpr double shapeGainDb = 12.0;

    inline BandSettings shapeBand (Shape shape, bool brickwall = false)
    {
        return { .inUse = true, .shape = shape, .frequency = 1000.0, .gain = shapeGainDb, .q = 1.0, .slope = 24.0, .brickwall = brickwall };
    }

    inline Settings oneBand (const BandSettings& band)
    {
        Settings settings;
        settings.bands[0] = band;
        return settings;
    }

    // A Dynamic Bell in slot 1, 12 dB down as the signal rises above Threshold.
    inline BandSettings dynamicBell()
    {
        auto band = bellBand (1000.0, 6.0, 1.0);
        band.dynamicRange = -12.0;
        band.threshold = -20.0;
        band.thresholdAuto = false;
        return band;
    }

    inline const char* shapeName (Shape shape)
    {
        switch (shape)
        {
            case Shape::Bell: return "bell";
            case Shape::LowShelf: return "low-shelf";
            case Shape::LowCut: return "low-cut";
            case Shape::HighShelf: return "high-shelf";
            case Shape::HighCut: return "high-cut";
            case Shape::Notch: return "notch";
            case Shape::BandPass: return "band-pass";
            case Shape::TiltShelf: return "tilt-shelf";
            case Shape::FlatTilt: return "flat-tilt";
            case Shape::AllPass: return "all-pass";
        }
        return "";
    }

    inline const char* placementName (StereoPlacement placement)
    {
        switch (placement)
        {
            case StereoPlacement::Stereo: return "stereo";
            case StereoPlacement::Left: return "left";
            case StereoPlacement::Right: return "right";
            case StereoPlacement::Mid: return "mid";
            case StereoPlacement::Side: return "side";
        }
        return "";
    }
} // namespace detail

inline std::vector<RenderScene> renderScenes()
{
    using namespace detail;
    std::vector<RenderScene> scenes;

    for (auto shape : allShapes)
        scenes.push_back ({ .name = std::string ("shape-") + shapeName (shape), .settings = oneBand (shapeBand (shape)) });
    for (auto shape : { Shape::LowCut, Shape::HighCut })
        scenes.push_back ({ .name = std::string ("shape-") + shapeName (shape) + "-brickwall", .settings = oneBand (shapeBand (shape, true)) });

    for (auto placement : allPlacements)
    {
        auto band = bellBand (1000.0, shapeGainDb, 1.0);
        band.placement = placement;
        scenes.push_back ({ .name = std::string ("placement-") + placementName (placement), .settings = oneBand (band) });
    }

    // Dynamic Bands: Band Detection Range with Auto Threshold, Free with a manual Threshold, and the
    // Sidechain as Detection Source.
    {
        auto band = dynamicBell();
        band.thresholdAuto = true;
        scenes.push_back ({ .name = "dynamic-band-range-auto-threshold", .settings = oneBand (band) });
    }
    {
        auto band = dynamicBell();
        band.detectionRange = DetectionRange::Free;
        band.detectionLow = 100.0;
        band.detectionHigh = 5000.0;
        scenes.push_back ({ .name = "dynamic-free-range-manual-threshold", .settings = oneBand (band) });
    }
    {
        auto band = dynamicBell();
        band.detectionSource = DetectionSource::External;
        scenes.push_back ({ .name = "dynamic-external-sidechain", .settings = oneBand (band), .sidechain = true });
    }

    scenes.push_back ({ .name = "24-bells", .settings = bells() });
    scenes.push_back ({ .name = "24-dynamic-bells", .settings = dynamicBells() });

    // Changes while rendering: a glide of Frequency and Gain, a crossfade between structures, and the
    // held states (Solo, Detection Audition, Global Bypass) switched on.
    scenes.push_back ({ .name = "glide", .settings = oneBand (bellBand (1000.0, 6.0, 1.0)), .change = oneBand (bellBand (2000.0, -6.0, 2.0)) });
    scenes.push_back ({ .name = "crossfade", .settings = oneBand (shapeBand (Shape::Bell)), .change = oneBand (shapeBand (Shape::LowCut)) });
    {
        auto held = bells();
        held.soloSlot = 12;
        scenes.push_back ({ .name = "solo", .settings = bells(), .change = held });
    }
    {
        auto held = dynamicBells();
        held.auditionSlot = 12;
        scenes.push_back ({ .name = "detection-audition", .settings = dynamicBells(), .change = held });
    }
    {
        auto held = bells();
        held.globalBypass = true;
        scenes.push_back ({ .name = "global-bypass", .settings = bells(), .change = held });
    }

    // The Output and the global settings.
    {
        auto settings = bells();
        settings.outputGainDb = 3.0;
        settings.outputPan = 0.3;
        settings.phaseInvert = true;
        scenes.push_back ({ .name = "output-gain-pan-invert", .settings = settings });
    }
    {
        auto settings = bells();
        settings.outputPan = -0.5;
        settings.panMode = PanMode::MidSide;
        scenes.push_back ({ .name = "output-pan-mid-side", .settings = settings });
    }
    {
        auto settings = bells();
        settings.autoGain = true;
        scenes.push_back ({ .name = "auto-gain", .settings = settings });
    }
    {
        auto settings = dynamicBells();
        settings.gainScale = 1.5;
        scenes.push_back ({ .name = "gain-scale", .settings = settings });
    }
    return scenes;
}

// Noise bursts, loud for 100 ms of every 200: full scale, so a tiny relative deviation is a measurable
// one, and quiet in between, so Dynamic Bands move.
inline float renderBurst (double seconds, float noise) { return std::fmod (seconds, 0.2) < 0.1 ? noise : 0.02f * noise; }

// Renders a scene at a sample rate: interleaved stereo samples, renderSeconds long.
inline std::vector<float> render (const RenderScene& scene, double sampleRate)
{
    const auto length = static_cast<long> (renderSeconds * sampleRate);
    const auto changeAt = static_cast<long> (renderChangeSeconds * sampleRate);
    Engine engine;
    engine.prepare (sampleRate, renderBlockSize, renderChannels);
    engine.setSettings (scene.settings);

    Noise mainNoise (1), sidechainNoise (2);
    std::vector<float> out;
    out.reserve (static_cast<size_t> (length) * renderChannels);
    std::array<std::vector<float>, renderChannels> main, sidechain;
    std::array<float*, renderChannels> mainChannels {};
    std::array<const float*, renderChannels> sidechainChannels {};
    for (int c = 0; c < renderChannels; ++c)
    {
        main[c].resize (renderBlockSize);
        sidechain[c].resize (renderBlockSize);
        mainChannels[c] = main[c].data();
        sidechainChannels[c] = sidechain[c].data();
    }

    bool changed = false;
    for (long start = 0; start < length; start += renderBlockSize)
    {
        if (scene.change && ! changed && start >= changeAt)
        {
            engine.setSettings (*scene.change);
            changed = true;
        }
        const int numSamples = static_cast<int> (std::min<long> (renderBlockSize, length - start));
        for (int i = 0; i < numSamples; ++i)
        {
            const double seconds = static_cast<double> (start + i) / sampleRate;
            for (int c = 0; c < renderChannels; ++c)
            {
                main[c][i] = renderBurst (seconds, mainNoise());
                // Bursts offset by half a period, so the Sidechain is loud while the main input is quiet.
                sidechain[c][i] = renderBurst (seconds + 0.1, sidechainNoise());
            }
        }
        const ConstAudioBlock sidechainBlock { sidechainChannels.data(), renderChannels, numSamples };
        engine.process ({ mainChannels.data(), renderChannels, numSamples }, scene.sidechain ? &sidechainBlock : nullptr);
        for (int i = 0; i < numSamples; ++i)
            for (int c = 0; c < renderChannels; ++c)
                out.push_back (main[c][i]);
    }
    return out;
}

} // namespace eq1::test
