// The null check's render scenes (tests/render/RenderScenes.h): what they cover, and that rendering
// one is repeatable, so a deviation between two builds is the Engine's and not the tool's.

#include "../render/Compare.h"
#include "../render/RenderScenes.h"
#include "Measure.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

using namespace eq1;

namespace
{
// True when some scene has a Band in use for which `wanted` holds.
bool someSceneHasBand (const std::function<bool (const BandSettings&)>& wanted)
{
    for (const auto& scene : test::renderScenes())
        for (const auto& band : scene.settings.bands)
            if (band.inUse && wanted (band))
                return true;
    return false;
}

bool someScene (const std::function<bool (const test::RenderScene&)>& wanted)
{
    const auto scenes = test::renderScenes();
    return std::any_of (scenes.begin(), scenes.end(), wanted);
}
} // namespace

TEST_CASE ("Render scenes cover every Shape, Brickwall included, and every Stereo Placement", "[render]")
{
    for (auto shape : test::allShapes)
        CHECK (someSceneHasBand ([shape] (const BandSettings& band) { return band.shape == shape && ! band.brickwall; }));
    for (auto shape : { Shape::LowCut, Shape::HighCut })
        CHECK (someSceneHasBand ([shape] (const BandSettings& band) { return band.shape == shape && band.brickwall; }));
    for (auto placement : test::allPlacements)
        CHECK (someSceneHasBand ([placement] (const BandSettings& band) { return band.placement == placement; }));
}

TEST_CASE ("Render scenes cover Dynamic Bands with each Detection Range, Threshold kind and Detection Source", "[render]")
{
    CHECK (someSceneHasBand ([] (const BandSettings& b) { return isDynamic (b) && b.detectionRange == DetectionRange::Band; }));
    CHECK (someSceneHasBand ([] (const BandSettings& b) { return isDynamic (b) && b.detectionRange == DetectionRange::Free; }));
    CHECK (someSceneHasBand ([] (const BandSettings& b) { return isDynamic (b) && b.thresholdAuto; }));
    CHECK (someSceneHasBand ([] (const BandSettings& b) { return isDynamic (b) && ! b.thresholdAuto; }));
    CHECK (someSceneHasBand ([] (const BandSettings& b) { return isDynamic (b) && b.detectionSource == DetectionSource::External; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.sidechain; }));
}

TEST_CASE ("Render scenes meter a moving Band and an idle one, and hear an External Band with no Sidechain", "[render]")
{
    // The Metered Band's detector, as the Detection Level shows it: one moving its Band, one only metered.
    const auto meteredBand = [] (const test::RenderScene& s) -> const BandSettings* {
        return s.settings.meteredSlot != 0 ? &s.settings.bands[static_cast<size_t> (s.settings.meteredSlot - 1)] : nullptr;
    };
    CHECK (someScene ([&] (const test::RenderScene& s) {
        const auto* band = meteredBand (s);
        return band != nullptr && band->inUse && isDynamic (*band) && ! band->dynamicsBypass;
    }));
    CHECK (someScene ([&] (const test::RenderScene& s) {
        const auto* band = meteredBand (s);
        return band != nullptr && band->inUse && hasDynamics (band->shape) && ! isDynamic (*band);
    }));
    // A detector with nothing to listen to.
    CHECK (someScene ([] (const test::RenderScene& s) {
        return ! s.sidechain && isDynamic (s.settings.bands[0]) && s.settings.bands[0].detectionSource == DetectionSource::External;
    }));
}

TEST_CASE ("Render scenes cover a glide, a structure crossfade, Solo, Detection Audition, Global Bypass and the Output", "[render]")
{
    // A change of Frequency or Gain glides; a change of Shape crossfades between structures.
    CHECK (someScene ([] (const test::RenderScene& s) {
        return s.change && s.change->bands[0].shape == s.settings.bands[0].shape
               && (s.change->bands[0].frequency != s.settings.bands[0].frequency || s.change->bands[0].gain != s.settings.bands[0].gain);
    }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.change && s.change->bands[0].shape != s.settings.bands[0].shape; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.change && s.change->soloSlot != 0; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.change && s.change->auditionSlot != 0; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.change && s.change->globalBypass; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.settings.outputGainDb != 0.0 && s.settings.outputPan != 0.0; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.settings.outputPan != 0.0 && s.settings.panMode == PanMode::MidSide; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.settings.autoGain; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.settings.phaseInvert; }));
    CHECK (someScene ([] (const test::RenderScene& s) { return s.settings.gainScale != 1.0; }));
}

TEST_CASE ("Render scene names are distinct file names, and the block size isn't a multiple of 16", "[render]")
{
    std::set<std::string> names;
    for (const auto& scene : test::renderScenes())
    {
        CHECK (names.insert (scene.name).second);
        CHECK (scene.name.find_first_not_of ("abcdefghijklmnopqrstuvwxyz0123456789-") == std::string::npos);
    }
    CHECK (test::renderBlockSize % 16 != 0);
    CHECK (test::renderSampleRates == std::array { 44100.0, 96000.0, 192000.0 });
}

TEST_CASE ("A render scene renders the same samples and Detection Level readings twice, and not silence", "[render]")
{
    const auto blocks = static_cast<size_t> (std::ceil (test::renderSeconds * 44100.0 / test::renderBlockSize));
    for (const auto& scene : test::renderScenes())
    {
        CAPTURE (scene.name);
        const auto first = test::render (scene, 44100.0);
        REQUIRE (first.samples.size() == static_cast<size_t> (test::renderSeconds * 44100.0) * test::renderChannels);
        CHECK (*std::max_element (first.samples.begin(), first.samples.end()) > 0.1f);
        // One reading per block: above the floor on a Metered Band's loud bursts, the floor without one.
        REQUIRE (first.detectionLevels.size() == blocks);
        const double loudest = *std::max_element (first.detectionLevels.begin(), first.detectionLevels.end());
        if (scene.settings.meteredSlot != 0)
            CHECK (loudest > -30.0);
        else
            CHECK (loudest == levelFloorDb);
        CHECK (test::render (scene, 44100.0) == first);
    }
}

TEST_CASE ("Comparing renders: exact, the worst deviation in dBFS, or lengths that differ", "[render]")
{
    std::vector<float> a (1000, 0.5f), b = a;
    CHECK (test::compare (a, b) == test::Comparison { .exact = true });

    b[500] = 0.5f + 1.0e-3f; // 1e-3 is -60 dBFS
    const auto deviation = test::compare (a, b);
    CHECK_FALSE (deviation.exact);
    CHECK (deviation.sameLength);
    CHECK (deviation.worstDb > -60.5);
    CHECK (deviation.worstDb < -59.5);
    CHECK_FALSE (deviation.passes (-120.0));
    CHECK (test::compare (a, b).worstDb == test::compare (b, a).worstDb);

    b[500] = 0.5f + 1.0e-7f; // about -140 dBFS, within the quality bar
    CHECK (test::compare (a, b).passes (-120.0));

    b.push_back (0.0f);
    CHECK_FALSE (test::compare (a, b).sameLength);
    CHECK_FALSE (test::compare (a, b).passes (-120.0));
}
