#pragma once

#include <juce_dsp/juce_dsp.h>

#include <memory>
#include <vector>

namespace eq1
{

// How quickly the Analyzer follows the signal.
enum class AnalyzerSpeed
{
    verySlow,
    slow,
    medium,
    fast,
    veryFast,
};

// How finely the Analyzer resolves Frequency: FFTs of 1024 to 8192 samples at 44.1 and 48 kHz.
enum class AnalyzerResolution
{
    low,
    medium,
    high,
    maximum,
};

// The Analyzer's view of one analysis tap: a smoothed spectrum and its Peak Hold, levels in dB where
// a full-scale sine reads 0 dB. Analyzer Tilt only changes the levels read for display. Message
// thread only.
class AnalyzerSpectrum
{
public:
    // The FFT size for a resolution, scaled with the sample rate so bins stay the same width in Hz.
    static int fftSize (double sampleRate, AnalyzerResolution resolution);

    // Allocates; forgets what was shown and held.
    void prepare (double sampleRate, AnalyzerResolution resolution);

    // Adds the newest samples from the tap.
    void push (const float* samples, int count);

    // Analyses the newest samples and moves the shown spectrum toward them, as for a frame seconds long.
    // With nothing pushed for a quarter second, it moves toward silence instead. Peak Hold rises at
    // once to the unsmoothed level of each frame above it, whatever the speed, and otherwise falls.
    void update (double seconds, AnalyzerSpeed speed);

    // The shown level at frequency, plus Analyzer Tilt: tiltDbPerOctave times the octaves above 1 kHz.
    double levelDb (double frequency, double tiltDbPerOctave) const;

    // Peak Hold's level at frequency, plus Analyzer Tilt as for levelDb.
    double heldLevelDb (double frequency, double tiltDbPerOctave) const;

    // Forgets what Peak Hold holds.
    void clearPeakHold();

    // The Frequency of the highest peak of the spectrum within a sixth of an octave of frequency, or
    // frequency itself when there is none.
    double peakNear (double frequency) const;

    bool isPrepared() const { return fft != nullptr; }
    double getSampleRate() const { return sampleRate; }
    AnalyzerResolution getResolution() const { return resolution; }

private:
    double binOf (double frequency) const { return frequency * size / sampleRate; }
    // The level of a per-bin power at frequency, plus Analyzer Tilt.
    double levelDbOf (const std::vector<double>& perBin, double frequency, double tiltDbPerOctave) const;

    double sampleRate = 48000.0;
    AnalyzerResolution resolution = AnalyzerResolution::medium;
    int size = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, history, transform;
    size_t historyWrite = 0;
    double secondsSilent = 0.0; // since samples were last pushed
    double windowGain = 1.0;
    std::vector<double> power; // per bin, smoothed
    std::vector<double> held; // per bin, Peak Hold
};

} // namespace eq1
