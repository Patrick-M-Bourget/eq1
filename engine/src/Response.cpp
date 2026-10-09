#include "eq1/Response.h"

#include "AutoGain.h"
#include "ShapeDesign.h"

#include <cmath>

namespace eq1
{

namespace
{
double magnitudeDb (const Cascade& cascade, double frequency, double sampleRate)
{
    return 20.0 * std::log10 (std::abs (responseAt (cascade, frequency, sampleRate)));
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
    estimate.start (settings, sampleRate);
    estimate.advance (AutoGainEstimate::numPoints);
    return estimate.db();
}

} // namespace eq1
