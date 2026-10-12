#pragma once

#include "eq1/Settings.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <vector>

namespace eq1::parameters
{

// Parameter IDs are stable: hosts store them in sessions and automation, so they never change.
// Each Band slot n (1 to 24) has band<n>_frequency, band<n>_gain, band<n>_q, band<n>_in_use, band<n>_bypass,
// band<n>_shape, band<n>_slope, band<n>_brickwall, band<n>_placement, and for dynamics
// band<n>_dynamic_range, band<n>_threshold, band<n>_threshold_auto, band<n>_attack, band<n>_release,
// band<n>_dynamics_bypass, band<n>_detection_source, band<n>_detection_range, band<n>_detection_low and
// band<n>_detection_high.
juce::String frequencyId (int slot);
juce::String gainId (int slot);
juce::String qId (int slot);
juce::String inUseId (int slot);
juce::String bypassId (int slot);
juce::String shapeId (int slot);
juce::String slopeId (int slot);
juce::String brickwallId (int slot);
juce::String placementId (int slot);
juce::String dynamicRangeId (int slot);
juce::String thresholdId (int slot);
juce::String thresholdAutoId (int slot);
juce::String attackId (int slot);
juce::String releaseId (int slot);
juce::String dynamicsBypassId (int slot);
juce::String detectionSourceId (int slot);
juce::String detectionRangeId (int slot);
juce::String detectionLowId (int slot);
juce::String detectionHighId (int slot);

// The whole-plugin controls.
inline const juce::String gainScaleId { "gain_scale" };
inline const juce::String autoGainId { "auto_gain" };
inline const juce::String outputGainId { "output_gain" };
inline const juce::String outputPanId { "output_pan" };
inline const juce::String panModeId { "pan_mode" };
inline const juce::String phaseInvertId { "phase_invert" };
inline const juce::String globalBypassId { "global_bypass" };

// Output Gain's bottom, in dB: silence, shown as -inf.
inline constexpr float outputGainSilentDb = static_cast<float> (parameterRanges::outputGain.low);

// The Shape choices, in the order of the Shape enum: Pro-Q 4's order, frozen by ADR 0003.
const juce::StringArray& shapeNames();

// The Stereo Placement choices, in the order of the StereoPlacement enum. Like Shape's, the order is
// frozen at release: hosts store the choice as a normalised value.
const juce::StringArray& placementNames();

// The Detection Source and Detection Range choices, in the order of their enums; frozen at release too.
const juce::StringArray& detectionSourceNames();
const juce::StringArray& detectionRangeNames();

// The Pan Mode choices, in the order of the PanMode enum; frozen at release too.
const juce::StringArray& panModeNames();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// A parameter's new normalised value.
struct NewValue
{
    juce::AudioProcessorParameter* parameter;
    float value;
};

// Sets several parameters as one edit the host sees: every gesture begins before the first value
// changes, and ends after the last.
void setTogether (const std::vector<NewValue>& values);

// Whether a switch parameter (a bool, or a choice of two) is on: from its raw value, readable from any
// thread, or from the parameter itself, whose value is current while its listeners are told of a change.
inline bool isOn (float value) { return value >= 0.5f; }
inline bool isOn (const std::atomic<float>* parameter) { return isOn (parameter->load()); }
inline bool isOn (const juce::AudioProcessorParameter& parameter) { return isOn (parameter.getValue()); }

// The parameter with this id: the one lookup the editor uses, so a missing id fails safely in a
// release build too. A missing id asserts in a debug build and, in any build, gives an inert parameter:
// one no processor holds, with no name, which no host, Engine, undo history or saved state ever sees,
// so reading, describing, attaching to or setting it is harmless. Message thread only.
juce::RangedAudioParameter& lookUp (juce::AudioProcessorValueTreeState& parameters, const juce::String& id);

// The host parameters of one Band slot, readable from any thread.
struct SlotValues
{
    std::atomic<float>* frequency;
    std::atomic<float>* gain;
    std::atomic<float>* q;
    std::atomic<float>* inUse;
    std::atomic<float>* bypass;
    std::atomic<float>* shape;
    std::atomic<float>* slope;
    std::atomic<float>* brickwall;
    std::atomic<float>* placement;
    std::atomic<float>* dynamicRange;
    std::atomic<float>* threshold;
    std::atomic<float>* thresholdAuto;
    std::atomic<float>* attack;
    std::atomic<float>* release;
    std::atomic<float>* dynamicsBypass;
    std::atomic<float>* detectionSource;
    std::atomic<float>* detectionRange;
    std::atomic<float>* detectionLow;
    std::atomic<float>* detectionHigh;

    static SlotValues of (juce::AudioProcessorValueTreeState& parameters, int slot);
    BandSettings read() const;
};

// The whole-plugin host parameters, readable from any thread.
struct OutputValues
{
    std::atomic<float>* gainScale;
    std::atomic<float>* autoGain;
    std::atomic<float>* outputGain;
    std::atomic<float>* outputPan;
    std::atomic<float>* panMode;
    std::atomic<float>* phaseInvert;
    std::atomic<float>* globalBypass;

    static OutputValues of (juce::AudioProcessorValueTreeState& parameters);
    // Sets the whole-plugin settings, leaving the Bands, Solo and Detection Audition alone.
    void readInto (Settings& settings) const;
};

// The host parameters as Settings, each resolved once: the one table of every Band Slot's and the
// whole-plugin parameters, which the processor owns (PluginProcessor::hostSettings) and the editor
// reads too. Reads from any thread, with no allocation or lock.
class SettingsReader
{
public:
    explicit SettingsReader (juce::AudioProcessorValueTreeState& parameters);

    // Every Band's and the whole-plugin settings. Solo, Detection Audition and the Metered Band are
    // not host parameters: they stay at 0.
    Settings read() const;
    // One Band Slot's (1 to 24) settings.
    BandSettings band (int slot) const { return slotValues (slot).read(); }
    // One Band Slot's (1 to 24) parameters, and the whole-plugin ones, for a read of only a few.
    const SlotValues& slotValues (int slot) const { return slots[static_cast<size_t> (slot - 1)]; }
    const OutputValues& outputValues() const { return outputs; }

private:
    std::array<SlotValues, numBandSlots> slots;
    OutputValues outputs;
};

} // namespace eq1::parameters
