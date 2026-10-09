// Loads a built eq1 plugin the way a host does, through its format wrapper (VST3 or AU), and checks
// that audio on the Sidechain bus reaches the Engine: a mono and a stereo Sidechain duck an External
// Dynamic Band, and the main input alone doesn't. Run by scripts/check.sh validate.
//
//   eq1_sidechain_routing_check <path to eq1.vst3>
//   eq1_sidechain_routing_check AU   (the AU must be installed and registered)

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <numbers>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormatManager& formats, const juce::String& what)
{
    for (int f = 0; f < formats.getNumFormats(); ++f)
    {
        auto* format = formats.getFormat (f);
        const bool au = what == "AU";
        if (au != (format->getName() == "AudioUnit") || (! au && format->getName() != "VST3"))
            continue;
        juce::StringArray candidates;
        if (au)
            candidates = format->searchPathsForPlugins ({}, false, false);
        else
            candidates.add (what);
        for (const auto& candidate : candidates)
        {
            juce::OwnedArray<juce::PluginDescription> types;
            format->findAllTypesForFile (types, candidate);
            for (auto* type : types)
                if (type->name == "eq1")
                {
                    juce::String error;
                    if (auto instance = formats.createPluginInstance (*type, sampleRate, blockSize, error))
                        return instance;
                    std::cerr << "Couldn't load " << type->fileOrIdentifier << ": " << error << "\n";
                }
        }
    }
    return nullptr;
}

// Sets a host parameter, found by name, to a plain value through the plugin's own text conversion.
bool set (juce::AudioProcessor& plugin, const juce::String& name, const juce::String& text)
{
    for (auto* parameter : plugin.getParameters())
        if (parameter->getName (100) == name)
        {
            parameter->setValueNotifyingHost (parameter->getValueForText (text));
            return true;
        }
    std::cerr << "No parameter " << name << "\n";
    return false;
}

// Level change in dB of a 100 Hz sine through the plugin, in the Side or the Mid of the main input,
// with a full-scale 60 Hz sine on every channel of the Sidechain when sidechainPlaying.
double gainDb (juce::AudioProcessor& plugin, bool side, bool sidechainPlaying)
{
    const int mainChannels = plugin.getMainBusNumInputChannels();
    const int channels = std::max (plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, blockSize);
    juce::MidiBuffer midi;
    double input = 0.0, output = 0.0;
    for (int block = 0, n = 0; block < 96; ++block)
    {
        buffer.clear();
        for (int i = 0; i < blockSize; ++i, ++n)
        {
            const auto s = static_cast<float> (0.25 * std::sin (2.0 * std::numbers::pi * 100.0 * n / sampleRate));
            buffer.setSample (0, i, s);
            if (mainChannels > 1)
                buffer.setSample (1, i, side ? -s : s);
            for (int ch = mainChannels; ch < plugin.getTotalNumInputChannels(); ++ch)
                buffer.setSample (ch, i, sidechainPlaying ? static_cast<float> (std::sin (2.0 * std::numbers::pi * 60.0 * n / sampleRate)) : 0.0f);
            if (block >= 64)
                input += s * s;
        }
        plugin.processBlock (buffer, midi);
        if (block >= 64)
            for (int i = 0; i < blockSize; ++i)
                output += buffer.getSample (0, i) * buffer.getSample (0, i);
    }
    return 10.0 * std::log10 (output / input);
}

int failures = 0;

void expect (const juce::String& what, double measured, double expected)
{
    const bool ok = std::abs (measured - expected) <= 0.5;
    failures += ok ? 0 : 1;
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << ": " << juce::String (measured, 2) << " dB (expected "
              << juce::String (expected, 1) << ")\n";
}
} // namespace

int main (int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "usage: eq1_sidechain_routing_check <eq1.vst3 path | AU>\n";
        return 2;
    }
    const juce::ScopedJuceInitialiser_GUI juce;
    juce::AudioPluginFormatManager formats;
    juce::addHeadlessDefaultFormatsToManager (formats);
    auto plugin = load (formats, argv[1]);
    if (plugin == nullptr)
    {
        std::cerr << "eq1 not found: " << argv[1] << "\n";
        return 1;
    }
    std::cout << "Sidechain routing through " << plugin->getPluginDescription().pluginFormatName << "\n";
    if (plugin->getBusCount (true) != 2)
    {
        std::cerr << "  FAIL  expected a main input and a Sidechain, found " << plugin->getBusCount (true) << " input buses\n";
        return 1;
    }

    const auto stereo = juce::AudioChannelSet::stereo();
    for (const auto& sidechain : { juce::AudioChannelSet::mono(), stereo, juce::AudioChannelSet::disabled() })
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (stereo);
        layout.inputBuses.add (sidechain);
        layout.outputBuses.add (stereo);
        const auto description = sidechain.isDisabled() ? juce::String ("no Sidechain") : sidechain.getDescription() + " Sidechain";
        if (! plugin->setBusesLayout (layout))
        {
            // Some wrappers can't switch a bus off; then there's nothing more to check for this layout.
            std::cout << (sidechain.isDisabled() ? "  skip  " : "  FAIL  ") << description << ": layout refused\n";
            failures += sidechain.isDisabled() ? 0 : 1;
            continue;
        }
        plugin->prepareToPlay (sampleRate, blockSize);
        // An External Bell at 100 Hz, its Free Detection Range 40 to 80 Hz.
        const bool ok = set (*plugin, "Band 1 In Use", "1") && set (*plugin, "Band 1 Frequency", "100")
                        && set (*plugin, "Band 1 Dynamic Range", "-10") && set (*plugin, "Band 1 Auto Threshold", "0")
                        && set (*plugin, "Band 1 Threshold", "-40") && set (*plugin, "Band 1 Detection Source", "External")
                        && set (*plugin, "Band 1 Detection Range", "Free") && set (*plugin, "Band 1 Detection Low", "40")
                        && set (*plugin, "Band 1 Detection High", "80") && set (*plugin, "Band 1 Stereo Placement", "Stereo");
        if (! ok)
            return 1;

        expect (description + ", main input alone", gainDb (*plugin, false, false), 0.0);
        const bool heard = ! sidechain.isDisabled();
        expect (description + ", kick on the Sidechain", gainDb (*plugin, false, true), heard ? -10.0 : 0.0);
        // A Side Band: a mono Sidechain drives it; a stereo one with the kick in its Mid doesn't.
        set (*plugin, "Band 1 Stereo Placement", "Side");
        gainDb (*plugin, true, false); // until the duck above has released
        expect (description + ", Side Band", gainDb (*plugin, true, true), sidechain == juce::AudioChannelSet::mono() ? -10.0 : 0.0);
        plugin->releaseResources();
    }
    std::cout << (failures == 0 ? "SUCCESS\n" : "FAILED\n");
    return failures == 0 ? 0 : 1;
}
