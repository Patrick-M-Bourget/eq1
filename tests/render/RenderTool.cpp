// The null check's render tool (docs/performance.md, "Null check"), run by scripts/check.sh null for
// two builds of the Engine, which compares what they rendered:
//
//   eq1_render render <dir>           writes every scene at every sample rate into <dir>, as raw
//                                     interleaved 32-bit floats named <scene>-<rate>.raw; a scene with a
//                                     Metered Band also writes its Detection Level readings, one 64-bit
//                                     float per block, as <scene>-<rate>-detection.raw
//   eq1_render compare <dir> <ref dir> prints each scene's worst deviation in dBFS, or "exact", and
//                                     fails above the bar (-120 dBFS), on a length that differs, or on
//                                     a scene in <ref dir> missing from <dir>; a scene only in <dir> is
//                                     "new" and passes. The Detection Level readings compare the same
//                                     way, in dB
//
// Linked against the Engine only (and Catch2's headers, for the shared Engine-test helpers).

#include "Compare.h"
#include "RenderScenes.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace eq1;
namespace fs = std::filesystem;

namespace
{

constexpr double barDb = -120.0;

std::string fileName (const std::string& scene, double sampleRate, const char* suffix = "")
{
    return scene + "-" + std::to_string (static_cast<int> (sampleRate)) + suffix + ".raw";
}

constexpr const char* detectionSuffix = "-detection";

bool metered (const test::RenderScene& scene) { return scene.settings.meteredSlot != 0 || (scene.change && scene.change->meteredSlot != 0); }

template <typename Sample>
bool write (const fs::path& file, const std::vector<Sample>& samples)
{
    std::ofstream out (file, std::ios::binary);
    out.write (reinterpret_cast<const char*> (samples.data()), static_cast<std::streamsize> (samples.size() * sizeof (Sample)));
    return static_cast<bool> (out);
}

template <typename Sample>
std::vector<Sample> read (const fs::path& file)
{
    std::ifstream in (file, std::ios::binary);
    std::vector<char> bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    std::vector<Sample> samples (bytes.size() / sizeof (Sample));
    std::copy (bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t> (samples.size() * sizeof (Sample)),
               reinterpret_cast<char*> (samples.data()));
    return samples;
}

int render (const fs::path& dir)
{
    fs::create_directories (dir);
    int written = 0;
    for (const auto& scene : test::renderScenes())
        for (double sampleRate : test::renderSampleRates)
        {
            const auto rendered = test::render (scene, sampleRate);
            const auto file = dir / fileName (scene.name, sampleRate), detectionFile = dir / fileName (scene.name, sampleRate, detectionSuffix);
            if (! write (file, rendered.samples) || (metered (scene) && ! write (detectionFile, rendered.detectionLevels)))
            {
                std::fprintf (stderr, "eq1_render: couldn't write %s\n", file.string().c_str());
                return 1;
            }
            ++written;
        }
    std::printf ("%d renders in %s\n", written, dir.string().c_str());
    return 0;
}

int compare (const fs::path& dir, const fs::path& refDir)
{
    bool failed = false;
    std::set<std::string> seen;
    // One line per file: the scene's output, or its Detection Level readings.
    const auto compareFile = [&] (const std::string& label, double sampleRate, const char* suffix, const char* unit, auto readFile) {
        const auto name = fileName (label, sampleRate, suffix);
        seen.insert (name);
        std::printf ("%-40s %6.1f kHz  ", (label + (*suffix != 0 ? " (Detection Level)" : "")).c_str(), sampleRate / 1000.0);
        if (! fs::exists (refDir / name))
        {
            std::printf ("new\n");
            return;
        }
        const auto comparison = test::compare (readFile (dir / name), readFile (refDir / name));
        if (! comparison.sameLength)
            std::printf ("FAIL: lengths differ\n");
        else if (comparison.exact)
            std::printf ("exact\n");
        else
            std::printf ("%7.1f %s%s\n", comparison.worstDb, unit, comparison.passes (barDb) ? "" : "  FAIL: above the bar");
        failed = failed || ! comparison.passes (barDb);
    };
    for (const auto& scene : test::renderScenes())
        for (double sampleRate : test::renderSampleRates)
        {
            compareFile (scene.name, sampleRate, "", "dBFS", read<float>);
            if (metered (scene))
                compareFile (scene.name, sampleRate, detectionSuffix, "dB", read<double>);
        }
    for (const auto& entry : fs::directory_iterator (refDir))
    {
        const auto name = entry.path().filename().string();
        if (entry.path().extension() == ".raw" && ! seen.count (name))
        {
            std::printf ("%-50s FAIL: missing from this tree\n", name.c_str());
            failed = true;
        }
    }
    std::printf ("\n%s\n", failed ? "Null check failed" : "Null check passed");
    return failed ? 1 : 0;
}

} // namespace

int main (int argc, char** argv)
{
    const std::string command = argc > 1 ? argv[1] : "";
    if (command == "render" && argc == 3)
        return render (argv[2]);
    if (command == "compare" && argc == 4)
        return compare (argv[2], argv[3]);
    std::fprintf (stderr, "usage: eq1_render render <dir> | compare <dir> <ref dir>\n");
    return 2;
}
