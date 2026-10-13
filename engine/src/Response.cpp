#include "eq1/Response.h"

#include "AutoGain.h"
#include "ShapeDesign.h"
#include "Units.h"

#include <cmath>

namespace eq1
{

namespace
{
double magnitudeDb (const Cascade& cascade, double frequency, double sampleRate)
{
    return gainToDecibels (std::abs (responseAt (cascade, frequency, sampleRate)));
}
} // namespace

double bandResponseDb (const BandSettings& band, double frequency, double sampleRate)
{
    return magnitudeDb (designBand (band, sampleRate), frequency, sampleRate);
}

void bandResponseDb (const BandSettings& band, const double* frequencies, double* db, int count, double sampleRate)
{
    const auto cascade = designBand (band, sampleRate);
    for (int i = 0; i < count; ++i)
        db[i] = magnitudeDb (cascade, frequencies[i], sampleRate);
}

double responseDb (const Settings& settings, double frequency, double sampleRate)
{
    double db = 0.0;
    for (const auto& band : settings.bands)
        if (band.inUse && ! band.bypass)
            db += bandResponseDb (scaledByGainScale (band, settings.gainScale), frequency, sampleRate);
    return db;
}

double autoGainDb (const Settings& settings, double sampleRate)
{
    AutoGainEstimate estimate;
    estimate.prepare (sampleRate);
    estimate.start (settings);
    estimate.advance (AutoGainEstimate::numPoints);
    return estimate.db();
}

Settings autoGainInputs (const Settings& settings)
{
    const BandSettings defaults;
    Settings inputs;
    for (size_t i = 0; i < inputs.bands.size(); ++i)
    {
        auto& band = inputs.bands[i];
        band = settings.bands[i];
        band.detectionSource = defaults.detectionSource;
        band.detectionRange = defaults.detectionRange;
        band.detectionLow = defaults.detectionLow;
        band.detectionHigh = defaults.detectionHigh;
    }
    inputs.gainScale = settings.gainScale;
    return inputs;
}

} // namespace eq1
