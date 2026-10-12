#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace eq1
{

// The sample rate the editor assumes before the host has given one (the processor's is 0 until then).
inline constexpr double assumedSampleRate = 48000.0;

// The processor's sample rate, or assumedSampleRate before it has one.
inline double sampleRateOrAssumed (const juce::AudioProcessor& processor)
{
    return processor.getSampleRate() > 0.0 ? processor.getSampleRate() : assumedSampleRate;
}

} // namespace eq1
